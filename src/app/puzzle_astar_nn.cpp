#include <sokoban_model.h>
#include <sokoban_heuristic.h>
#include <sokoban_board.h>

#include <torch/torch.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>
#include <cmath>

namespace sf = std::filesystem;
namespace s  = std;
namespace so = sokoban;

// ---------------------------------------------------------------------------
// NeuralHeuristic — wraps a loaded libtorch model, satisfies so::Heuristic
// ---------------------------------------------------------------------------
template <typename ModelType>
struct NeuralHeuristic {
  ModelType model;

  explicit NeuralHeuristic(const sf::path& weights_path) {
    torch::load(model, weights_path.string());
    model->eval();
  }

  int operator()(so::SokobanBoard& board) {
    torch::NoGradGuard no_grad;
    auto input = encode_state(board).unsqueeze(0);       // [1, 5, H, W]
    float cost = model->forward(input).template item<float>();
    return (int)s::floor(s::max(0.f, cost));
  }
};

// ---------------------------------------------------------------------------
// A* (player-move graph)
// ---------------------------------------------------------------------------
struct Node {
  so::BoardState state;
  int            g;  // moves so far
};

template <so::Heuristic H>
int astar(so::SokobanBoard& game, H& h) {
  auto cmp = [&](const Node& a, const Node& b) {
    game.set_state(a.state);  int ha = h(game);
    game.set_state(b.state);  int hb = h(game);
    // max-heap; return true when a is worse (larger f) than b
    return (a.g + ha) > (b.g + hb);
  };
  s::priority_queue<Node, s::vector<Node>, decltype(cmp)> open(cmp);

  s::unordered_map<so::BoardState, int, s::hash<so::BoardState>> best;

  so::BoardState init = game.state();
  open.push({init, 0});
  best[init] = 0;

  while (not open.empty()) {
    Node cur = open.top(); open.pop();

    game.set_state(cur.state);
    if (game.is_terminal()) return cur.g;

    if (best.count(cur.state) and best[cur.state] < cur.g) continue;

    for (const so::Dr& dir : so::DIRECTIONS) {
      game.set_state(cur.state);
      if (game.play(dir)) {
        int ng = cur.g + 1;
        auto it = best.find(game.state());
        if (it == best.end() or it->second > ng) {
          best[game.state()] = ng;
          open.push({game.state(), ng});
        }
      }
    }
  }
  return -1;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    s::cerr << "usage: puzzle_astar_nn <puzzle.bc> <weights.pt> [cnn|attn]\n";
    return 1;
  }

  sf::path  source(argv[1]);
  sf::path  weights(argv[2]);
  s::string arch = (argc >= 4) ? argv[3] : "cnn";

  so::SokobanBoard game(source, /*disable_history=*/true);

  int result = -1;
  if (arch == "attn") {
    NeuralHeuristic<SokobanAttn> h(weights);
    result = astar(game, h);
  } else {
    NeuralHeuristic<SokobanCNN> h(weights);
    result = astar(game, h);
  }

  if (result < 0)
    s::cout << "no solution found\n";
  else
    s::cout << "solved in " << result << " moves\n";

  return 0;
}
