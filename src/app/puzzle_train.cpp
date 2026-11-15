// Neural heuristic trainer (DeepCubeA-style approximate value iteration).
//
// Each iteration:
//   1. generate N states by random backward (pull) walks from solved states of
//      training levels; walk length k is an upper bound on push distance
//   2. optionally tighten labels with a one-step Bellman backup using the
//      network frozen at the start of the iteration:
//         y = min(k, 1 + min_{s'} h(s'))      (y = 1 if a successor is solved)
//      and clamp from below with the admissible lone-box matching bound
//   3. train the network on (state, y) for E epochs
//   4. periodically: exact-distance validation on held-out levels, batched
//      weighted A* solve rate on held-out puzzle start states, checkpoint
//
// Metrics are appended as JSON lines to <out>/metrics.jsonl.

#include <sokoban_model.h>
#include <sokoban_fast.h>
#include <level_set.h>
#include <batch_search.h>

#include <CLI/CLI.hpp>
#include <torch/torch.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace sf = std::filesystem;
namespace s  = std;
namespace so = sokoban;

using Clock = s::chrono::steady_clock;

static s::atomic<bool> g_stop{false};
static void on_signal(int){ g_stop = true; }

struct Opts {
  s::string data_dir = "data/boxoban";
  s::string out_dir  = "runs/default";
  ModelConfig model;
  int    iters        = 60;
  int    samples      = 131072;   // states generated per iteration
  int    epochs       = 1;        // passes over each iteration's data
  int    batch        = 512;
  double lr           = 1e-3;
  double lr_min       = 2e-5;
  double wd           = 1e-4;
  int    warmup       = 300;      // steps
  s::string loss      = "mse";    // mse | huber | asym
  double w_over       = 2.0;      // asym: overestimation weight
  int    kmax_start   = 20;
  int    kmax_end     = 120;
  double kmax_ramp    = 0.5;      // fraction of iters to reach kmax_end
  int    bellman_from = 4;        // first iteration using Bellman targets (0=never)
  bool   lb_clamp     = true;
  int    threads      = 16;
  int    eval_every   = 5;
  int    eval_levels  = 64;
  long   eval_budget  = 1000;
  double eval_weight  = 1.0;
  int    eval_batch   = 16;
  int    val_levels   = 32;
  int    seed         = 1;
  s::string init      = "";       // warm-start weights (.pt with .cfg)
};

// ---------------------------------------------------------------------------
template <typename F>
void parallel_for(long n, int threads, F&& fn){
  if (threads <= 1 or n < 64){ for (long i = 0; i < n; ++i) fn(i, 0); return; }
  s::vector<s::thread> pool;
  s::atomic<long> next{0};
  for (int t = 0; t < threads; ++t)
    pool.emplace_back([&, t]{
      long i;
      while ((i = next.fetch_add(256)) < n)
        for (long j = i; j < s::min(n, i + 256); ++j) fn(j, t);
    });
  for (auto& th : pool) th.join();
}

struct JsonLine {
  s::ostringstream o;
  bool first = true;
  JsonLine(){ o << "{"; o << s::setprecision(6); }
  template <typename T> JsonLine& kv(const s::string& k, const T& v){
    o << (first ? "" : ",") << "\"" << k << "\":";
    if constexpr (s::is_convertible_v<T, s::string>) o << "\"" << v << "\"";
    else if constexpr (s::is_floating_point_v<T>){
      if (s::isfinite(v)) o << v; else o << "null";
    }
    else o << v;
    first = false;
    return *this;
  }
  s::string str(){ return o.str() + "}"; }
};

// ---------------------------------------------------------------------------
struct ValSet {
  s::vector<int>           level;  // index into test levels
  s::vector<so::FastState> state;
  s::vector<float>         dist;   // exact push distance
};

static ValSet build_val_set(const s::vector<so::FastLevel>& test, int nlevels,
                            int seed){
  ValSet v;
  s::mt19937 rng(seed);
  int n = s::min<int>(nlevels, test.size());
  s::vector<ValSet> per(n);
  parallel_for(n, 16, [&](long li, int){
    auto layers = so::exact_distance_layers(test[li], 40, 150000);
    s::map<int, s::vector<so::FastState>> by_d;
    for (auto& [st, d] : layers) by_d[d].push_back(st);
    s::mt19937 r(seed * 7919 + li);
    for (auto& [d, vec] : by_d){
      s::shuffle(vec.begin(), vec.end(), r);
      for (int k = 0; k < s::min<int>(6, vec.size()); ++k){
        per[li].level.push_back((int)li);
        per[li].state.push_back(vec[k]);
        per[li].dist.push_back((float)d);
      }
    }
  });
  for (auto& p : per){
    v.level.insert(v.level.end(), p.level.begin(), p.level.end());
    v.state.insert(v.state.end(), p.state.begin(), p.state.end());
    v.dist.insert(v.dist.end(), p.dist.begin(), p.dist.end());
  }
  return v;
}

struct EvalSummary {
  double solve_rate = 0, mean_expanded = 0, mean_pushes = 0, mean_secs = 0;
  int solved = 0, total = 0;
};

static EvalSummary run_eval(const s::vector<so::FastLevel>& levels, int n,
                            const BatchEval& ev, const SearchConfig& sc){
  EvalSummary e;
  n = s::min<int>(n, levels.size());
  double secs = 0;
  for (int i = 0; i < n and not g_stop; ++i){
    auto r = batch_wastar(levels[i], ev, sc);
    e.total++;
    secs += r.seconds;
    if (r.solved){
      e.solved++;
      e.mean_expanded += r.expanded;
      e.mean_pushes   += r.pushes;
    }
  }
  if (e.solved){ e.mean_expanded /= e.solved; e.mean_pushes /= e.solved; }
  e.solve_rate = e.total ? (double)e.solved / e.total : 0;
  e.mean_secs = e.total ? secs / e.total : 0;
  return e;
}

static torch::Tensor loss_fn(const Opts& o, torch::Tensor pred, torch::Tensor y){
  if (o.loss == "huber") return torch::smooth_l1_loss(pred, y, at::Reduction::Mean, 1.0);
  if (o.loss == "asym"){
    auto over  = torch::relu(pred - y);
    auto under = torch::relu(y - pred);
    return (o.w_over * over.pow(2) + under.pow(2)).mean();
  }
  return torch::mse_loss(pred, y);
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]){
  Opts o;
  CLI::App app{"Train a neural push-distance heuristic for Sokoban"};
  app.add_option("--data", o.data_dir, "level directory (recursive .bc)");
  app.add_option("--out", o.out_dir, "run output directory");
  app.add_option("--arch", o.model.arch)->check(CLI::IsMember({"cnn", "resnet", "attn"}));
  app.add_option("--channels", o.model.channels);
  app.add_option("--blocks", o.model.blocks);
  app.add_option("--hidden", o.model.hidden);
  app.add_option("--head", o.model.head)->check(CLI::IsMember({"flat", "gap"}));
  app.add_option("--heads", o.model.heads);
  app.add_flag("--se", o.model.se, "squeeze-excitation global context in res blocks");
  app.add_flag("--dilate", o.model.dilate, "dilation cycle 1,2,4 in res blocks");
  app.add_option("--iters", o.iters);
  app.add_option("--samples", o.samples, "states generated per iteration");
  app.add_option("--epochs", o.epochs, "passes over each iteration's data");
  app.add_option("--batch", o.batch);
  app.add_option("--lr", o.lr);
  app.add_option("--lr-min", o.lr_min);
  app.add_option("--wd", o.wd);
  app.add_option("--warmup", o.warmup);
  app.add_option("--loss", o.loss)->check(CLI::IsMember({"mse", "huber", "asym"}));
  app.add_option("--w-over", o.w_over);
  app.add_option("--kmax-start", o.kmax_start);
  app.add_option("--kmax-end", o.kmax_end);
  app.add_option("--kmax-ramp", o.kmax_ramp);
  app.add_option("--bellman-from", o.bellman_from, "0 disables Bellman targets");
  app.add_option("--lb-clamp", o.lb_clamp, "clamp targets below by matching bound");
  app.add_option("--threads", o.threads);
  app.add_option("--eval-every", o.eval_every);
  app.add_option("--eval-levels", o.eval_levels);
  app.add_option("--eval-budget", o.eval_budget, "max node expansions per puzzle");
  app.add_option("--eval-weight", o.eval_weight);
  app.add_option("--eval-batch", o.eval_batch);
  app.add_option("--val-levels", o.val_levels);
  app.add_option("--seed", o.seed);
  app.add_option("--init", o.init, "warm-start from weights (.pt + .cfg)");
  app.add_option("--amp", g_use_amp, "bf16 autocast on CUDA (1/0)");
  CLI11_PARSE(app, argc, argv);

  s::signal(SIGINT, on_signal);
  s::signal(SIGTERM, on_signal);
  torch::manual_seed(o.seed);
  at::globalContext().setAllowTF32CuBLAS(true);
  at::globalContext().setAllowTF32CuDNN(true);
  // no cudnn benchmark: search and Bellman batches vary in size, and each new
  // shape would trigger a fresh autotune

  torch::Device dev = torch::cuda::is_available() ? torch::Device(torch::kCUDA)
                                                  : torch::Device(torch::kCPU);
  sf::path out(o.out_dir);
  sf::create_directories(out);
  {
    s::ofstream cfg(out / "args.txt");
    for (int i = 0; i < argc; ++i) cfg << argv[i] << (i + 1 < argc ? " " : "\n");
  }
  s::ofstream mlog(out / "metrics.jsonl", s::ios::app);
  auto emit = [&](JsonLine& j){
    s::string line = j.str();
    mlog << line << "\n" << s::flush;
    s::cout << line << "\n" << s::flush;
  };

  LevelSet ls = load_levels(o.data_dir);
  if (ls.train.empty() or ls.test.empty()){
    s::cerr << "need both train and test levels\n";
    return 1;
  }

  auto t_start = Clock::now();
  auto elapsed = [&]{ return s::chrono::duration<double>(Clock::now() - t_start).count(); };

  // ----- model
  s::shared_ptr<HeuristicNet> model;
  if (not o.init.empty()){
    model = load_model(o.init, &o.model);
    s::cerr << "warm start from " << o.init << "\n";
  } else {
    model = make_model(o.model);
  }
  model->to(dev);
  long nparams = count_params(model);
  s::ofstream(out / "model.cfg") << o.model.serialize();

  // ----- validation data
  ValSet val = build_val_set(ls.test, o.val_levels, 12345);
  s::cerr << "val set: " << val.state.size() << " exact-distance states\n";

  SearchConfig sc;
  sc.weight = o.eval_weight;
  sc.batch = o.eval_batch;
  sc.max_expanded = o.eval_budget;

  {
    JsonLine j;
    j.kv("event", "start").kv("tag", o.model.tag()).kv("params", nparams)
     .kv("train_levels", (long)ls.train.size()).kv("test_levels", (long)ls.test.size())
     .kv("val_states", (long)val.state.size()).kv("device", dev.is_cuda() ? "cuda" : "cpu");
    emit(j);
  }
  // classical baseline on the same validation / eval sets, for comparison
  {
    double abs_err = 0, bias = 0;
    for (size_t i = 0; i < val.state.size(); ++i){
      double e = ls.test[val.level[i]].matching_cost(val.state[i]) - val.dist[i];
      abs_err += s::abs(e); bias += e;
    }
    double nval = s::max<size_t>(1, val.state.size());
    JsonLine j;
    j.kv("event", "baseline_val").kv("heuristic", "matching")
     .kv("val_mae", abs_err / nval).kv("val_bias", bias / nval)
     .kv("val_dist_mean", s::accumulate(val.dist.begin(), val.dist.end(), 0.0) / nval);
    emit(j);
  }
  if (o.eval_every > 0){
    EvalSummary b = run_eval(ls.test, o.eval_levels, matching_eval, sc);
    JsonLine j;
    j.kv("event", "baseline").kv("heuristic", "matching")
     .kv("solve_rate", b.solve_rate).kv("mean_expanded", b.mean_expanded)
     .kv("mean_pushes", b.mean_pushes).kv("mean_secs", b.mean_secs);
    emit(j);
  }

  torch::optim::AdamW opt(model->parameters(),
                          torch::optim::AdamWOptions(o.lr).weight_decay(o.wd));
  long total_steps = (long)o.iters * o.epochs * ((o.samples + o.batch - 1) / o.batch);
  long step = 0;
  double best_solve = -1, best_mae = 1e9;

  auto eval_model = [&](const so::FastLevel& L, const s::vector<so::FastState>& st){
    return predict(*model, dev, L, st);
  };

  for (int it = 1; it <= o.iters and not g_stop; ++it){
    double t_it = elapsed();
    double ramp = s::min(1.0, (double)(it - 1) / s::max(1.0, o.kmax_ramp * o.iters));
    int kmax = (int)s::lround(o.kmax_start + ramp * (o.kmax_end - o.kmax_start));

    // ----- 1. generate states
    int N = o.samples;
    s::vector<so::FastState> st(N);
    s::vector<int>   lvl(N);
    s::vector<float> y(N);
    s::vector<float> walk(N);
    {
      s::vector<s::mt19937_64> rngs;
      for (int t = 0; t < o.threads; ++t)
        rngs.emplace_back(so::mix64((uint64_t)o.seed * 1000003ULL + it * 131ULL + t));
      parallel_for(N, o.threads, [&](long i, int t){
        auto& r = rngs[t];
        int li = s::uniform_int_distribution<int>(0, (int)ls.train.size() - 1)(r);
        int k  = s::uniform_int_distribution<int>(0, kmax)(r);
        auto [s_, a] = so::random_pull_walk(ls.train[li], k, r);
        st[i] = s_; lvl[i] = li; walk[i] = (float)a; y[i] = (float)a;
      });
    }
    double t_gen = elapsed() - t_it;

    // ----- 2. Bellman backup with the frozen network
    double improved = 0, t_bell = 0;
    if (o.bellman_from > 0 and it >= o.bellman_from){
      double t0 = elapsed();
      model->eval();
      const int CH = 8192;
      for (int lo = 0; lo < N; lo += CH){
        int hi = s::min(N, lo + CH);
        // successors of each state in [lo, hi)
        s::vector<s::vector<so::FastState>> succ(hi - lo);
        s::vector<char> goal_child(hi - lo, 0);
        parallel_for(hi - lo, o.threads, [&](long k, int){
          const auto& L = ls.train[lvl[lo + k]];
          if (L.is_goal(st[lo + k])){ goal_child[k] = 2; return; }
          L.for_successors(st[lo + k], [&](const so::FastState& n){
            if (L.is_goal(n)) goal_child[k] = 1;
            else succ[k].push_back(n);
          });
        });
        s::vector<so::FastState> flat;
        s::vector<int> flat_lvl, owner;
        for (int k = 0; k < hi - lo; ++k)
          for (auto& n : succ[k]){ flat.push_back(n); flat_lvl.push_back(lvl[lo + k]); owner.push_back(k); }
        s::vector<float> best(hi - lo, 1e9f);
        {
          torch::NoGradGuard ng;
          AmpGuard amp(dev.is_cuda());
          const long PC = 16384;
          for (long a = 0; a < (long)flat.size(); a += PC){
            long b = s::min<long>(flat.size(), a + PC);
            s::vector<so::FastState> ps(flat.begin() + a, flat.begin() + b);
            s::vector<int> pl(flat_lvl.begin() + a, flat_lvl.begin() + b);
            auto pred = model->forward(encode_batch(ls.train, pl, ps, o.threads).to(dev))
                            .clamp_min(0).to(torch::kCPU, torch::kFloat).contiguous();
            const float* p = pred.data_ptr<float>();
            for (long q = a; q < b; ++q)
              best[owner[q]] = s::min(best[owner[q]], p[q - a]);
          }
        }
        for (int k = 0; k < hi - lo; ++k){
          int i = lo + k;
          float target = y[i];
          if (goal_child[k] == 2) target = 0;
          else if (goal_child[k] == 1) target = 1;
          else if (best[k] < 1e8f) target = s::min(target, 1.f + best[k]);
          if (target < y[i] - 1e-3f) improved += 1;
          y[i] = target;
        }
      }
      improved /= N;
      model->train();
      t_bell = elapsed() - t0;
    }
    if (o.lb_clamp){
      parallel_for(N, o.threads, [&](long i, int){
        int lb = ls.train[lvl[i]].matching_cost(st[i]);
        if (lb < so::FastLevel::INF) y[i] = s::max(y[i], (float)lb);
      });
    }

    // ----- 3. train
    model->train();
    double t0 = elapsed();
    double loss_sum = 0; long nb = 0;
    double ymean = s::accumulate(y.begin(), y.end(), 0.0) / N;
    double wmean = s::accumulate(walk.begin(), walk.end(), 0.0) / N;
    s::vector<int> perm(N);
    s::iota(perm.begin(), perm.end(), 0);
    s::mt19937 prng(o.seed * 31 + it);
    double cur_lr = o.lr;
    for (int ep = 0; ep < o.epochs and not g_stop; ++ep){
      s::shuffle(perm.begin(), perm.end(), prng);
      for (int lo = 0; lo + o.batch <= N and not g_stop; lo += o.batch){
        s::vector<so::FastState> bs(o.batch);
        s::vector<int> bl(o.batch);
        s::vector<float> by(o.batch);
        for (int k = 0; k < o.batch; ++k){
          int i = perm[lo + k];
          bs[k] = st[i]; bl[k] = lvl[i]; by[k] = y[i];
        }
        // lr: linear warmup then cosine to lr_min
        if (step < o.warmup) cur_lr = o.lr * (step + 1) / o.warmup;
        else {
          double pr = (double)(step - o.warmup) / s::max(1L, total_steps - o.warmup);
          cur_lr = o.lr_min + 0.5 * (o.lr - o.lr_min) * (1 + s::cos(M_PI * s::min(1.0, pr)));
        }
        for (auto& g : opt.param_groups())
          static_cast<torch::optim::AdamWOptions&>(g.options()).lr(cur_lr);

        auto x = encode_batch(ls.train, bl, bs, 1).to(dev, /*non_blocking=*/true);
        auto t = torch::from_blob(by.data(), {o.batch}).clone().to(dev);
        opt.zero_grad();
        torch::Tensor pred;
        {
          AmpGuard amp(dev.is_cuda());
          pred = model->forward(x);
        }
        auto loss = loss_fn(o, pred.to(torch::kFloat), t);
        loss.backward();
        torch::nn::utils::clip_grad_norm_(model->parameters(), 1.0);
        opt.step();
        if (step % 20 == 0){ loss_sum += loss.item<double>(); ++nb; } // sync rarely
        ++step;
      }
    }
    double loss_avg = nb ? loss_sum / nb : NAN;
    double t_train = elapsed() - t0;

    JsonLine j;
    j.kv("event", "iter").kv("iter", it).kv("time", elapsed()).kv("step", step)
     .kv("kmax", kmax).kv("lr", cur_lr).kv("loss", loss_avg)
     .kv("target_mean", ymean).kv("walk_mean", wmean).kv("bellman_improved", improved)
     .kv("t_gen", t_gen).kv("t_bellman", t_bell).kv("t_train", t_train);

    // ----- 4. validation / eval / checkpoint
    bool do_eval = o.eval_every > 0 and (it % o.eval_every == 0 or it == o.iters or g_stop);
    {
      // exact-distance validation (cheap, every iteration)
      model->eval();
      double abs_err = 0, bias = 0, under_rate = 0;
      s::map<int, s::vector<size_t>> by_level;
      for (size_t i = 0; i < val.state.size(); ++i) by_level[val.level[i]].push_back(i);
      for (auto& [li, idx] : by_level){
        s::vector<so::FastState> vs;
        for (auto i : idx) vs.push_back(val.state[i]);
        auto pr = predict(*model, dev, ls.test[li], vs);
        for (size_t k = 0; k < idx.size(); ++k){
          double e = pr[k] - val.dist[idx[k]];
          abs_err += s::abs(e); bias += e; under_rate += e <= 0.5;
        }
      }
      double nval = s::max<size_t>(1, val.state.size());
      j.kv("val_mae", abs_err / nval).kv("val_bias", bias / nval)
       .kv("val_admissible_frac", under_rate / nval);
      if (abs_err / nval < best_mae) best_mae = abs_err / nval;

      if (do_eval){
        EvalSummary e = run_eval(ls.test, o.eval_levels, eval_model, sc);
        j.kv("solve_rate", e.solve_rate).kv("mean_expanded", e.mean_expanded)
         .kv("mean_pushes", e.mean_pushes).kv("eval_secs", e.mean_secs);
        if (e.solve_rate > best_solve){
          best_solve = e.solve_rate;
          save_model(model, o.model, out / "best.pt");
        }
      }
      model->train();
    }
    save_model(model, o.model, out / "latest.pt");
    emit(j);
  }

  JsonLine j;
  j.kv("event", "done").kv("time", elapsed()).kv("best_solve_rate", best_solve)
   .kv("best_val_mae", best_mae).kv("interrupted", (bool)g_stop ? 1 : 0);
  emit(j);
  return 0;
}
