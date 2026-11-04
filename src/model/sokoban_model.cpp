#include <sokoban_model.h>

// ---------------------------------------------------------------------------
// encode_state
// ---------------------------------------------------------------------------
torch::Tensor encode_state(const so::SokobanBoard& board,
                            const so::BoardState&   st) {
  auto t = torch::zeros({N_CHANNELS, BOARD_H, BOARD_W});
  auto a = t.accessor<float, 3>();

  for (const so::Pt& p : board.walls())
    a[0][p.x][p.y] = 1.f;

  for (const so::Pt& p : board.goals())
    a[2][p.x][p.y] = 1.f;

  for (const so::Pt& p : st.boxes)
    a[3][p.x][p.y] = 1.f;

  a[4][st.player.x][st.player.y] = 1.f;

  // ch 1: walkable floor cells not occupied by any entity
  for (const so::Pt& p : board.spaces()) {
    if (board.goals().find(p)  == board.goals().end()  and
        st.boxes.find(p)       == st.boxes.end()       and
        p                      != st.player)
      a[1][p.x][p.y] = 1.f;
  }

  return t;
}

torch::Tensor encode_state(const so::SokobanBoard& board) {
  return encode_state(board, board.state());
}

// ---------------------------------------------------------------------------
// SokobanCNN
// ---------------------------------------------------------------------------
SokobanCNNImpl::SokobanCNNImpl() {
  conv1 = register_module("conv1",
      torch::nn::Conv2d(torch::nn::Conv2dOptions(N_CHANNELS, 32, 3).padding(1)));
  bn1 = register_module("bn1", torch::nn::BatchNorm2d(32));

  conv2 = register_module("conv2",
      torch::nn::Conv2d(torch::nn::Conv2dOptions(32, 64, 3).padding(1)));
  bn2 = register_module("bn2", torch::nn::BatchNorm2d(64));

  conv3 = register_module("conv3",
      torch::nn::Conv2d(torch::nn::Conv2dOptions(64, 128, 3).padding(1)));
  bn3 = register_module("bn3", torch::nn::BatchNorm2d(128));

  fc1 = register_module("fc1", torch::nn::Linear(128, 256));
  fc2 = register_module("fc2", torch::nn::Linear(256,   1));
}

torch::Tensor SokobanCNNImpl::forward(torch::Tensor x) {
  // x: [B, 5, 10, 10]
  x = torch::relu(bn1->forward(conv1->forward(x)));  // [B, 32, 10, 10]
  x = torch::relu(bn2->forward(conv2->forward(x)));  // [B, 64, 10, 10]
  x = torch::relu(bn3->forward(conv3->forward(x)));  // [B, 128, 10, 10]
  x = x.mean({2, 3});                                // [B, 128]  global avg pool
  x = torch::relu(fc1->forward(x));                  // [B, 256]
  return fc2->forward(x).squeeze(1);                 // [B]
}

// ---------------------------------------------------------------------------
// SokobanAttn
// ---------------------------------------------------------------------------
SokobanAttnImpl::SokobanAttnImpl() {
  // Projects the 5 binary channel values per cell to D dimensions.
  // Weight shape [5, D] acts as an embedding matrix: each column is the
  // embedding for one channel; we sum over active channels via matmul.
  channel_proj = register_module("channel_proj",
      torch::nn::Linear(torch::nn::LinearOptions(N_CHANNELS, D).bias(false)));

  pos_embed = register_module("pos_embed",
      torch::nn::Embedding(N_TOKENS, D));

  auto enc_layer = torch::nn::TransformerEncoderLayer(
      torch::nn::TransformerEncoderLayerOptions(D, N_HEADS)
          .dim_feedforward(256)
          .dropout(0.1));
  encoder = register_module("encoder",
      torch::nn::TransformerEncoder(enc_layer, N_LAYERS));

  fc1 = register_module("fc1", torch::nn::Linear(D, 128));
  fc2 = register_module("fc2", torch::nn::Linear(128,  1));
}

torch::Tensor SokobanAttnImpl::forward(torch::Tensor x) {
  // x: [B, 5, H, W]
  int B = x.size(0);

  // Reshape to [B, N_TOKENS, N_CHANNELS] and project to [B, N_TOKENS, D]
  auto flat = x.permute({0, 2, 3, 1})                // [B, H, W, 5]
               .reshape({B, N_TOKENS, N_CHANNELS});   // [B, 100, 5]
  auto tokens = channel_proj->forward(flat);          // [B, 100, D]

  // Add learnable positional embeddings
  auto pos_idx = torch::arange(N_TOKENS, x.options().dtype(torch::kLong));
  tokens = tokens + pos_embed->forward(pos_idx)       // [B, 100, D]
                     .unsqueeze(0).expand({B, -1, -1});

  // TransformerEncoder expects [seq, B, D] (seq-first, no batch_first option)
  auto seq = tokens.permute({1, 0, 2});               // [100, B, D]
  seq = encoder->forward(seq);                        // [100, B, D]

  // Mean-pool over token sequence
  auto pooled = seq.mean(0);                          // [B, D]
  pooled = torch::relu(fc1->forward(pooled));         // [B, 128]
  return fc2->forward(pooled).squeeze(1);             // [B]
}
