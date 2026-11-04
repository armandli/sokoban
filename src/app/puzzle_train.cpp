#include <sokoban_model.h>
#include <data_generator.h>
#include <asymmetric_loss.h>

#include <torch/torch.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

namespace sf = std::filesystem;
namespace s  = std;

static constexpr int   BATCH_SIZE       = 128;
static constexpr int   EPOCHS           = 200;
static constexpr int   STEPS_PER_EPOCH  = 500;
static constexpr int   DEPTH_START      = 5;
static constexpr int   DEPTH_END        = 50;
static constexpr float LR               = 1e-3f;
static constexpr float W_OVER           = 4.f;
static constexpr float W_UNDER          = 1.f;
static constexpr int   SAVE_EVERY       = 10;

template <typename Model>
void train_model(Model& model, DataGenerator& gen,
                 const sf::path& save_dir, const s::string& prefix) {
  model->train();
  torch::optim::Adam opt(model->parameters(),
                         torch::optim::AdamOptions(LR));

  for (int epoch = 1; epoch <= EPOCHS; ++epoch) {
    float progress  = (float)(epoch - 1) / s::max(1, EPOCHS - 1);
    int   max_depth = (int)s::round(
        DEPTH_START + progress * (DEPTH_END - DEPTH_START));
    gen.set_max_depth(max_depth);

    double epoch_loss = 0.0;

    for (int step = 0; step < STEPS_PER_EPOCH; ++step) {
      auto batch = gen.next_batch(BATCH_SIZE);

      s::vector<torch::Tensor> encoded;
      encoded.reserve(BATCH_SIZE);
      for (int i = 0; i < (int)batch.states.size(); ++i)
        encoded.push_back(encode_state(gen.board_at(batch.board_indices[i]),
                                       batch.states[i]));

      torch::Tensor input  = torch::stack(encoded, 0);  // [B, 5, H, W]
      torch::Tensor target = torch::tensor(
          s::vector<float>(batch.costs.begin(), batch.costs.end()));  // [B]

      opt.zero_grad();
      torch::Tensor pred = model->forward(input);
      torch::Tensor loss = asymmetric_mse(pred, target, W_OVER, W_UNDER);
      loss.backward();
      opt.step();

      epoch_loss += loss.item<double>();
    }

    double avg = epoch_loss / STEPS_PER_EPOCH;
    s::cout << "epoch " << epoch
            << "  depth_cap " << max_depth
            << "  avg_loss "  << avg << "\n";

    if (epoch % SAVE_EVERY == 0) {
      sf::path p = save_dir / (prefix + "_ep" + s::to_string(epoch) + ".pt");
      torch::save(model, p.string());
      s::cout << "  saved " << p << "\n";
    }
  }

  sf::path final_path = save_dir / (prefix + "_final.pt");
  torch::save(model, final_path.string());
  s::cout << "final model saved to " << final_path << "\n";
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    s::cerr << "usage: puzzle_train <data_dir> <save_dir> [cnn|attn]\n";
    return 1;
  }

  sf::path  data_dir = argv[1];
  sf::path  save_dir = argv[2];
  s::string arch     = (argc >= 4) ? argv[3] : "cnn";

  sf::create_directories(save_dir);

  DataGenerator gen(data_dir, DEPTH_START);

  if (arch == "attn") {
    SokobanAttn model;
    train_model(model, gen, save_dir, "attn");
  } else {
    SokobanCNN model;
    train_model(model, gen, save_dir, "cnn");
  }

  return 0;
}
