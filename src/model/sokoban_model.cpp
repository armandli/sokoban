#include <sokoban_model.h>

#include <ATen/autocast_mode.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

// ---------------------------------------------------------------------------
// encoding
// ---------------------------------------------------------------------------
void encode_fast(const so::FastLevel& L, const so::FastState& st, float* out){
  s::memset(out, 0, sizeof(float) * ENC_SIZE);
  so::u128 region = L.reach(st.boxes, st.player);
  for (int i = 0; i < PLANE; ++i){
    if (not so::test(L.floor, i)) out[0 * PLANE + i] = 1.f;
    if (so::test(L.goals, i))     out[1 * PLANE + i] = 1.f;
    if (so::test(st.boxes, i))    out[2 * PLANE + i] = 1.f;
    if (so::test(region, i))      out[3 * PLANE + i] = 1.f;
    if (so::test(L.dead, i))      out[4 * PLANE + i] = 1.f;
  }
}

torch::Tensor encode_batch(const so::FastLevel& L,
                           const s::vector<so::FastState>& states){
  auto t = torch::empty({(long)states.size(), N_IN, BOARD_H, BOARD_W});
  float* p = t.data_ptr<float>();
  for (size_t i = 0; i < states.size(); ++i) encode_fast(L, states[i], p + i * ENC_SIZE);
  return t;
}

torch::Tensor encode_batch(const s::vector<so::FastLevel>& levels,
                           const s::vector<int>& level_of,
                           const s::vector<so::FastState>& states,
                           int threads){
  long n = (long)states.size();
  auto t = torch::empty({n, N_IN, BOARD_H, BOARD_W});
  float* p = t.data_ptr<float>();
  auto work = [&](long lo, long hi){
    for (long i = lo; i < hi; ++i)
      encode_fast(levels[level_of[i]], states[i], p + i * ENC_SIZE);
  };
  if (threads <= 1 or n < 2048){ work(0, n); return t; }
  s::vector<s::thread> pool;
  long step = (n + threads - 1) / threads;
  for (long lo = 0; lo < n; lo += step) pool.emplace_back(work, lo, s::min(n, lo + step));
  for (auto& th : pool) th.join();
  return t;
}

// ---------------------------------------------------------------------------
// config
// ---------------------------------------------------------------------------
s::string ModelConfig::serialize() const {
  s::ostringstream o;
  o << "arch=" << arch << "\nchannels=" << channels << "\nblocks=" << blocks
    << "\nhidden=" << hidden << "\nhead=" << head << "\nheads=" << heads
    << "\nse=" << (int)se << "\ndilate=" << (int)dilate << "\n";
  return o.str();
}

ModelConfig ModelConfig::parse(const s::string& text){
  ModelConfig c;
  s::istringstream in(text);
  s::string line;
  while (s::getline(in, line)){
    auto eq = line.find('=');
    if (eq == s::string::npos) continue;
    s::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "arch") c.arch = v;
    else if (k == "channels") c.channels = s::stoi(v);
    else if (k == "blocks")   c.blocks   = s::stoi(v);
    else if (k == "hidden")   c.hidden   = s::stoi(v);
    else if (k == "head")     c.head     = v;
    else if (k == "heads")    c.heads    = s::stoi(v);
    else if (k == "se")       c.se       = s::stoi(v) != 0;
    else if (k == "dilate")   c.dilate   = s::stoi(v) != 0;
  }
  return c;
}

s::string ModelConfig::tag() const {
  s::ostringstream o;
  o << arch << "_c" << channels << "_b" << blocks;
  if (arch != "attn") o << "_" << head;
  if (arch == "attn") o << "_h" << heads;
  if (se) o << "_se";
  if (dilate) o << "_dil";
  return o.str();
}

// ---------------------------------------------------------------------------
// value head shared by conv nets
// ---------------------------------------------------------------------------
struct ConvHeadImpl : torch::nn::Module {
  bool flat;
  torch::nn::Conv2d      proj{nullptr};
  torch::nn::BatchNorm2d bn{nullptr};
  torch::nn::Linear      fc1{nullptr}, fc2{nullptr};

  ConvHeadImpl(int ch, int hidden, bool flat_head): flat(flat_head) {
    if (flat){
      proj = register_module("proj", torch::nn::Conv2d(
          torch::nn::Conv2dOptions(ch, 8, 1).bias(false)));
      bn  = register_module("bn", torch::nn::BatchNorm2d(8));
      fc1 = register_module("fc1", torch::nn::Linear(8 * PLANE, hidden));
    } else {
      fc1 = register_module("fc1", torch::nn::Linear(ch, hidden));
    }
    fc2 = register_module("fc2", torch::nn::Linear(hidden, 1));
  }

  torch::Tensor forward(torch::Tensor x){
    if (flat) x = torch::relu(bn(proj(x))).flatten(1);
    else      x = x.mean({2, 3});
    return fc2(torch::relu(fc1(x))).squeeze(1);
  }
};
TORCH_MODULE(ConvHead);

// ---------------------------------------------------------------------------
// plain CNN (the original design: conv-BN-ReLU stack + head)
// ---------------------------------------------------------------------------
struct PlainCNN : HeuristicNet {
  torch::nn::Sequential body{nullptr};
  ConvHead head{nullptr};

  explicit PlainCNN(const ModelConfig& c){
    body = register_module("body", torch::nn::Sequential());
    int in = N_IN;
    for (int i = 0; i < c.blocks; ++i){
      body->push_back(torch::nn::Conv2d(
          torch::nn::Conv2dOptions(in, c.channels, 3).padding(1).bias(false)));
      body->push_back(torch::nn::BatchNorm2d(c.channels));
      body->push_back(torch::nn::ReLU());
      in = c.channels;
    }
    head = register_module("head", ConvHead(c.channels, c.hidden, c.head == "flat"));
  }

  torch::Tensor forward(torch::Tensor x) override { return head(body->forward(x)); }
};

// ---------------------------------------------------------------------------
// ResNet tower
// ---------------------------------------------------------------------------
struct ResBlockImpl : torch::nn::Module {
  torch::nn::Conv2d      c1{nullptr}, c2{nullptr};
  torch::nn::BatchNorm2d b1{nullptr}, b2{nullptr};
  torch::nn::Linear      se1{nullptr}, se2{nullptr};
  bool use_se;

  ResBlockImpl(int ch, int dilation, bool se): use_se(se) {
    c1 = register_module("c1", torch::nn::Conv2d(
        torch::nn::Conv2dOptions(ch, ch, 3).padding(dilation).dilation(dilation).bias(false)));
    b1 = register_module("b1", torch::nn::BatchNorm2d(ch));
    c2 = register_module("c2", torch::nn::Conv2d(
        torch::nn::Conv2dOptions(ch, ch, 3).padding(1).bias(false)));
    b2 = register_module("b2", torch::nn::BatchNorm2d(ch));
    if (use_se){
      // global pooling -> per-channel scale and bias (KataGo-style global ctx)
      se1 = register_module("se1", torch::nn::Linear(2 * ch, ch / 2));
      se2 = register_module("se2", torch::nn::Linear(ch / 2, 2 * ch));
    }
  }

  torch::Tensor forward(torch::Tensor x){
    auto y = torch::relu(b1(c1(x)));
    y = b2(c2(y));
    if (use_se){
      auto pooled = torch::cat({y.mean({2, 3}), s::get<0>(y.flatten(2).max(2))}, 1);
      auto gb = se2(torch::relu(se1(pooled)));   // [B, 2C]
      auto parts = gb.chunk(2, 1);
      y = y * torch::sigmoid(parts[0]).unsqueeze(2).unsqueeze(3) +
          parts[1].unsqueeze(2).unsqueeze(3);
    }
    return torch::relu(x + y);
  }
};
TORCH_MODULE(ResBlock);

struct ResNet : HeuristicNet {
  torch::nn::Conv2d      stem{nullptr};
  torch::nn::BatchNorm2d stem_bn{nullptr};
  torch::nn::ModuleList  tower{nullptr};
  ConvHead head{nullptr};

  explicit ResNet(const ModelConfig& c){
    stem = register_module("stem", torch::nn::Conv2d(
        torch::nn::Conv2dOptions(N_IN, c.channels, 3).padding(1).bias(false)));
    stem_bn = register_module("stem_bn", torch::nn::BatchNorm2d(c.channels));
    tower = register_module("tower", torch::nn::ModuleList());
    const int cycle[3] = {1, 2, 4};
    for (int i = 0; i < c.blocks; ++i)
      tower->push_back(ResBlock(c.channels, c.dilate ? cycle[i % 3] : 1, c.se));
    head = register_module("head", ConvHead(c.channels, c.hidden, c.head == "flat"));
  }

  torch::Tensor forward(torch::Tensor x) override {
    x = torch::relu(stem_bn(stem(x)));
    for (auto& m : *tower) x = m->as<ResBlockImpl>()->forward(x);
    return head(x);
  }
};

// ---------------------------------------------------------------------------
// Pre-norm transformer over the 100 cells + CLS token
// (libtorch's TransformerEncoderLayer has no norm_first, so hand-rolled)
// ---------------------------------------------------------------------------
struct AttnBlockImpl : torch::nn::Module {
  torch::nn::LayerNorm ln1{nullptr}, ln2{nullptr};
  torch::nn::MultiheadAttention attn{nullptr};
  torch::nn::Linear fc1{nullptr}, fc2{nullptr};

  AttnBlockImpl(int d, int heads){
    ln1  = register_module("ln1", torch::nn::LayerNorm(torch::nn::LayerNormOptions({d})));
    ln2  = register_module("ln2", torch::nn::LayerNorm(torch::nn::LayerNormOptions({d})));
    attn = register_module("attn", torch::nn::MultiheadAttention(
        torch::nn::MultiheadAttentionOptions(d, heads).dropout(0.0)));
    fc1  = register_module("fc1", torch::nn::Linear(d, 4 * d));
    fc2  = register_module("fc2", torch::nn::Linear(4 * d, d));
  }

  // x: [S, B, D]
  torch::Tensor forward(torch::Tensor x){
    auto h = ln1(x);
    x = x + s::get<0>(attn->forward(h, h, h, torch::Tensor(), /*need_weights=*/false));
    return x + fc2(torch::gelu(fc1(ln2(x))));
  }
};
TORCH_MODULE(AttnBlock);

struct AttnNet : HeuristicNet {
  torch::nn::Linear     embed{nullptr};
  torch::Tensor         pos, cls;
  torch::nn::ModuleList blocks{nullptr};
  torch::nn::LayerNorm  ln{nullptr};
  torch::nn::Linear     fc1{nullptr}, fc2{nullptr};

  explicit AttnNet(const ModelConfig& c){
    int d = c.channels;
    embed  = register_module("embed", torch::nn::Linear(N_IN, d));
    pos    = register_parameter("pos", torch::randn({PLANE, 1, d}) * 0.02);
    cls    = register_parameter("cls", torch::randn({1, 1, d}) * 0.02);
    blocks = register_module("blocks", torch::nn::ModuleList());
    for (int i = 0; i < c.blocks; ++i) blocks->push_back(AttnBlock(d, c.heads));
    ln  = register_module("ln", torch::nn::LayerNorm(torch::nn::LayerNormOptions({d})));
    fc1 = register_module("fc1", torch::nn::Linear(d, c.hidden));
    fc2 = register_module("fc2", torch::nn::Linear(c.hidden, 1));
  }

  torch::Tensor forward(torch::Tensor x) override {
    long B = x.size(0);
    auto tok = embed(x.flatten(2).permute({2, 0, 1})) + pos;  // [100, B, D]
    tok = torch::cat({cls.expand({1, B, cls.size(2)}), tok}, 0);
    for (auto& m : *blocks) tok = m->as<AttnBlockImpl>()->forward(tok);
    auto z = ln(tok[0]);                                        // CLS [B, D]
    return fc2(torch::relu(fc1(z))).squeeze(1);
  }
};

// ---------------------------------------------------------------------------
s::shared_ptr<HeuristicNet> make_model(const ModelConfig& c){
  if (c.arch == "cnn")    return s::make_shared<PlainCNN>(c);
  if (c.arch == "resnet") return s::make_shared<ResNet>(c);
  if (c.arch == "attn")   return s::make_shared<AttnNet>(c);
  throw s::runtime_error("unknown arch " + c.arch);
}

static sf::path cfg_path_for(const sf::path& pt){
  sf::path p = pt;
  return p.replace_extension(".cfg");
}

void save_model(const s::shared_ptr<HeuristicNet>& m, const ModelConfig& cfg,
                const sf::path& pt_path){
  // write to temp then rename so a concurrent reader never sees a partial file
  sf::path tmp = pt_path; tmp += ".tmp";
  torch::serialize::OutputArchive ar;
  m->save(ar);
  ar.save_to(tmp.string());
  sf::rename(tmp, pt_path);
  s::ofstream(cfg_path_for(pt_path)) << cfg.serialize();
}

s::shared_ptr<HeuristicNet> load_model(const sf::path& pt_path, ModelConfig* cfg_out){
  s::ifstream in(cfg_path_for(pt_path));
  if (not in) throw s::runtime_error("missing " + cfg_path_for(pt_path).string());
  s::stringstream ss; ss << in.rdbuf();
  ModelConfig cfg = ModelConfig::parse(ss.str());
  auto m = make_model(cfg);
  torch::serialize::InputArchive ar;
  ar.load_from(pt_path.string());
  m->load(ar);
  if (cfg_out) *cfg_out = cfg;
  return m;
}

bool g_use_amp = false;

AmpGuard::AmpGuard(bool enable): on(enable and g_use_amp) {
  if (on){
    at::autocast::set_autocast_dtype(at::kCUDA, at::kBFloat16);
    at::autocast::set_autocast_enabled(at::kCUDA, true);
  }
}

AmpGuard::~AmpGuard(){
  if (on){
    at::autocast::set_autocast_enabled(at::kCUDA, false);
    at::autocast::clear_cache();
  }
}

long count_params(const s::shared_ptr<HeuristicNet>& m){
  long n = 0;
  for (auto& p : m->parameters()) n += p.numel();
  return n;
}

s::vector<float> predict(HeuristicNet& m, const torch::Device& dev,
                         const so::FastLevel& L,
                         const s::vector<so::FastState>& states, int chunk){
  torch::NoGradGuard ng;
  AmpGuard amp(dev.is_cuda());
  s::vector<float> out;
  out.reserve(states.size());
  for (size_t lo = 0; lo < states.size(); lo += chunk){
    size_t hi = s::min(states.size(), lo + (size_t)chunk);
    s::vector<so::FastState> part(states.begin() + lo, states.begin() + hi);
    auto y = m.forward(encode_batch(L, part).to(dev))
                 .to(torch::kCPU, torch::kFloat).contiguous();
    const float* p = y.data_ptr<float>();
    out.insert(out.end(), p, p + y.size(0));
  }
  return out;
}
