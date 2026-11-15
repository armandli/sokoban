// Solve Sokoban puzzles with batched weighted A* over the push graph using a
// trained neural heuristic (or the classical matching heuristic).
//
//   puzzle_astar_nn --weights runs/x/best.pt data/boxoban/hard/bx0001.bc
//   puzzle_astar_nn --weights runs/x/best.pt --eval-dir data/boxoban --split test
//   puzzle_astar_nn --heuristic matching --eval-dir data/boxoban --split test

#include <sokoban_model.h>
#include <sokoban_fast.h>
#include <level_set.h>
#include <batch_search.h>

#include <CLI/CLI.hpp>
#include <torch/torch.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace sf = std::filesystem;
namespace s  = std;
namespace so = sokoban;

static void print_state(const so::FastLevel& L, const so::FastState& st){
  so::u128 region = L.reach(st.boxes, st.player);
  for (int r = 0; r < so::FAST_H; ++r){
    for (int c = 0; c < so::FAST_W; ++c){
      int i = r * so::FAST_W + c;
      bool g = so::test(L.goals, i), b = so::test(st.boxes, i);
      char ch = not so::test(L.floor, i) ? 'W'
              : b ? (g ? 'C' : 'B')
              : g ? 'G'
              : so::test(region, i) ? '.' : ' ';
      if (i == st.player) ch = g ? 'Q' : 'P';
      s::cout << ch;
    }
    s::cout << "\n";
  }
}

int main(int argc, char* argv[]){
  s::string weights, heuristic = "nn", eval_dir, split = "test", json_out;
  s::string combine = "none";
  s::vector<s::string> puzzles;
  SearchConfig sc;
  int limit = 0;
  bool show = false;

  CLI::App app{"Batched weighted A* Sokoban solver"};
  app.add_option("puzzles", puzzles, "puzzle .bc files");
  app.add_option("--weights", weights, "model weights (.pt with .cfg)");
  app.add_option("--heuristic", heuristic)->check(CLI::IsMember({"nn", "matching"}));
  app.add_option("--combine", combine, "nn only: 'max' uses max(nn, matching lower bound)")
     ->check(CLI::IsMember({"none", "max"}));
  app.add_option("--eval-dir", eval_dir, "evaluate on a level directory split");
  app.add_option("--split", split)->check(CLI::IsMember({"test", "train", "all"}));
  app.add_option("--limit", limit, "max puzzles to evaluate (0 = all)");
  app.add_option("--weight", sc.weight, "f = g + weight * h");
  app.add_option("--batch", sc.batch, "nodes expanded per heuristic call");
  app.add_option("--budget", sc.max_expanded, "max node expansions");
  app.add_option("--json", json_out, "write summary JSON here");
  app.add_flag("--show", show, "print the solution path");
  app.add_option("--amp", g_use_amp, "bf16 autocast on CUDA (1/0)");
  CLI11_PARSE(app, argc, argv);

  torch::Device dev = torch::cuda::is_available() ? torch::Device(torch::kCUDA)
                                                  : torch::Device(torch::kCPU);
  s::shared_ptr<HeuristicNet> model;
  BatchEval ev = matching_eval;
  if (heuristic == "nn"){
    if (weights.empty()){ s::cerr << "--weights required for nn heuristic\n"; return 1; }
    model = load_model(weights);
    model->to(dev);
    model->eval();
    ev = [&](const so::FastLevel& L, const s::vector<so::FastState>& st){
      auto h = predict(*model, dev, L, st);
      if (combine == "max"){
        auto lb = matching_eval(L, st);
        for (size_t i = 0; i < h.size(); ++i) h[i] = s::max(h[i], lb[i]);
      }
      return h;
    };
  }

  s::vector<so::FastLevel> levels;
  if (not eval_dir.empty()){
    LevelSet ls = load_levels(eval_dir);
    if (split != "train") levels.insert(levels.end(), ls.test.begin(), ls.test.end());
    if (split != "test")  levels.insert(levels.end(), ls.train.begin(), ls.train.end());
  }
  for (auto& p : puzzles){
    so::SokobanBoard b(p, true);
    if (not so::FastLevel::fits(b)){ s::cerr << p << ": larger than 10x10, skipped\n"; continue; }
    levels.emplace_back(b, sf::path(p).stem().string());
  }
  if (limit > 0 and (int)levels.size() > limit) levels.erase(levels.begin() + limit, levels.end());

  int solved = 0;
  double exp_sum = 0, push_sum = 0, sec_sum = 0;
  for (auto& L : levels){
    auto r = batch_wastar(L, ev, sc);
    sec_sum += r.seconds;
    if (r.solved){ ++solved; exp_sum += r.expanded; push_sum += r.pushes; }
    if (levels.size() <= 20 or not r.solved)
      s::cout << L.name << ": " << (r.solved ? "solved in " + s::to_string(r.pushes) + " pushes"
                                              : s::string("NOT solved"))
              << ", expanded " << r.expanded << ", " << s::fixed << s::setprecision(2)
              << r.seconds << "s\n";
    if (show and r.solved)
      for (auto& st : r.path){ print_state(L, st); s::cout << "\n"; }
  }
  double n = s::max<size_t>(1, levels.size());
  s::cout << s::fixed << "summary: solved " << solved << "/" << levels.size()
          << " (" << s::setprecision(1) << 100.0 * solved / n << "%)"
          << ", mean expanded " << s::setprecision(0) << (solved ? exp_sum / solved : 0)
          << ", mean pushes " << s::setprecision(1) << (solved ? push_sum / solved : 0)
          << ", mean time " << s::setprecision(3) << sec_sum / n << "s\n";
  if (not json_out.empty()){
    s::ofstream j(json_out);
    j << "{\"heuristic\":\"" << heuristic << "\",\"combine\":\"" << combine
      << "\",\"weights\":\"" << weights
      << "\",\"n\":" << levels.size() << ",\"solved\":" << solved
      << ",\"solve_rate\":" << solved / n
      << ",\"mean_expanded\":" << (solved ? exp_sum / solved : 0)
      << ",\"mean_pushes\":" << (solved ? push_sum / solved : 0)
      << ",\"mean_secs\":" << sec_sum / n << ",\"budget\":" << sc.max_expanded
      << ",\"weight\":" << sc.weight << ",\"batch\":" << sc.batch << "}\n";
  }
  return 0;
}
