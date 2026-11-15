#ifndef SOKOBAN_MODEL_H
#define SOKOBAN_MODEL_H

#include <sokoban_fast.h>

#include <torch/torch.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace s  = std;
namespace sf = std::filesystem;
namespace so = sokoban;

// Input encoding: N_IN binary planes over the fixed 10x10 grid.
//   0 wall / outside level      1 goal
//   2 box                       3 player-reachable region
//   4 dead square (box can never reach a goal from here)
// Player position is canonicalised in FastState, so the reachable region
// replaces an explicit player plane.
constexpr int BOARD_H = so::FAST_H;
constexpr int BOARD_W = so::FAST_W;
constexpr int N_IN    = 5;
constexpr int PLANE   = BOARD_H * BOARD_W;
constexpr int ENC_SIZE = N_IN * PLANE;

// write ENC_SIZE floats for one state into out
void encode_fast(const so::FastLevel& L, const so::FastState& st, float* out);

// [n, N_IN, H, W] CPU float tensor
torch::Tensor encode_batch(const so::FastLevel& L,
                           const s::vector<so::FastState>& states);

// Mixed-level batch: level_of[i] indexes into levels
torch::Tensor encode_batch(const s::vector<so::FastLevel>& levels,
                           const s::vector<int>& level_of,
                           const s::vector<so::FastState>& states,
                           int threads = 1);

struct ModelConfig {
  s::string arch     = "resnet"; // cnn | resnet | attn
  int       channels = 64;       // conv width, or d_model for attn
  int       blocks   = 6;        // conv layers / residual blocks / attn layers
  int       hidden   = 256;      // value head width
  s::string head     = "flat";   // flat | gap   (resnet / cnn)
  int       heads    = 4;        // attention heads (attn)
  bool      se       = false;    // squeeze-excitation global context (resnet)
  bool      dilate   = false;    // dilation cycle 1,2,4 (resnet)

  s::string serialize() const;
  static ModelConfig parse(const s::string& text);
  s::string tag() const; // short human-readable id
};

struct HeuristicNet : torch::nn::Module {
  // x: [B, N_IN, H, W] -> [B] predicted remaining pushes
  virtual torch::Tensor forward(torch::Tensor x) = 0;
};

s::shared_ptr<HeuristicNet> make_model(const ModelConfig& cfg);

// <path>.pt holds weights, <path>.cfg holds the ModelConfig
void save_model(const s::shared_ptr<HeuristicNet>& m, const ModelConfig& cfg,
                const sf::path& pt_path);
s::shared_ptr<HeuristicNet> load_model(const sf::path& pt_path,
                                       ModelConfig* cfg_out = nullptr);

// bf16 autocast on CUDA (RAII).  Enabled when `on` and global AMP is set.
extern bool g_use_amp;
struct AmpGuard {
  bool on;
  explicit AmpGuard(bool enable = true);
  ~AmpGuard();
};

long count_params(const s::shared_ptr<HeuristicNet>& m);

// Run the network in eval mode / no-grad over states in chunks.
s::vector<float> predict(HeuristicNet& m, const torch::Device& dev,
                         const so::FastLevel& L,
                         const s::vector<so::FastState>& states,
                         int chunk = 4096);

#endif // SOKOBAN_MODEL_H
