#ifndef SOKOBAN_MODEL_H
#define SOKOBAN_MODEL_H

#include <sokoban_board.h>
#include <sokoban_state.h>

#include <torch/torch.h>

namespace s  = std;
namespace so = sokoban;

// Input encoding: 5 binary channels over a BOARD_H x BOARD_W grid.
//   ch 0  wall
//   ch 1  empty walkable floor (not goal, not box, not player)
//   ch 2  goal cell (regardless of occupant)
//   ch 3  box (on or off goal)
//   ch 4  player (on or off goal)
// Channels 2+3 both set => box on goal; 2+4 => player on goal.
static constexpr int BOARD_H    = 10;
static constexpr int BOARD_W    = 10;
static constexpr int N_CHANNELS = 5;

// Encode the current board state into a [N_CHANNELS, BOARD_H, BOARD_W] tensor.
torch::Tensor encode_state(const so::SokobanBoard& board);

// Encode a BoardState given the board's static geometry separately (used when
// we do not want to call set_state on the live board).
torch::Tensor encode_state(const so::SokobanBoard& board,
                            const so::BoardState&   st);

// ---------------------------------------------------------------------------
// CNN model
//   Input  : [B, 5, 10, 10]
//   Network: 3 conv-BN-ReLU blocks (5->32->64->128) with same-padding,
//            global-average-pool, two FC layers
//   Output : [B]  estimated remaining box-push cost
// ---------------------------------------------------------------------------
struct SokobanCNNImpl : torch::nn::Module {
  torch::nn::Conv2d     conv1{nullptr}, conv2{nullptr}, conv3{nullptr};
  torch::nn::BatchNorm2d bn1{nullptr},  bn2{nullptr},   bn3{nullptr};
  torch::nn::Linear     fc1{nullptr},  fc2{nullptr};

  SokobanCNNImpl();
  torch::Tensor forward(torch::Tensor x);
};
TORCH_MODULE(SokobanCNN);

// ---------------------------------------------------------------------------
// Attention model (ViT-style)
//   Input  : [B, 5, 10, 10]
//   Tokens : one per cell (100 tokens).  Each token = sum of active-channel
//            embeddings (dim D) + learnable 2-D positional embedding (dim D).
//   Network: 4-layer TransformerEncoder (4 heads, D=64, FFN=256), mean-pool,
//            two FC layers
//   Output : [B]  estimated remaining box-push cost
// ---------------------------------------------------------------------------
struct SokobanAttnImpl : torch::nn::Module {
  static constexpr int D       = 64;
  static constexpr int N_HEADS = 4;
  static constexpr int N_LAYERS = 4;
  static constexpr int N_TOKENS = BOARD_H * BOARD_W;

  // channel_embed weight [N_CHANNELS, D] is used as a projection matrix
  // (not standard Embedding lookup — we multiply float binary channels by it)
  torch::nn::Linear    channel_proj{nullptr};
  torch::nn::Embedding pos_embed{nullptr};
  torch::nn::TransformerEncoder encoder{nullptr};
  torch::nn::Linear    fc1{nullptr}, fc2{nullptr};

  SokobanAttnImpl();
  torch::Tensor forward(torch::Tensor x);
};
TORCH_MODULE(SokobanAttn);

#endif // SOKOBAN_MODEL_H
