#ifndef DATA_GENERATOR_H
#define DATA_GENERATOR_H

#include <sokoban_board.h>

#include <filesystem>
#include <random>
#include <vector>
#include <utility>

namespace s  = std;
namespace sf = std::filesystem;
namespace so = sokoban;

// Generates (BoardState, cost_k) training pairs via backward random walks.
//
// Starting from each puzzle's goal state (all boxes on goals), we apply k
// random box-pull moves.  The resulting state is at known distance <= k from
// the goal.  Because k is an upper bound on the true cost, the asymmetric loss
// in the training loop compensates by biasing the network toward
// underestimation.
struct DataGenerator {
  s::vector<so::SokobanBoard> boards;
  int max_depth;
  s::mt19937 rng;

  // Load every .bc file found (recursively) under data_dir.
  DataGenerator(const sf::path& data_dir, int max_depth, uint32_t seed = 42);

  int board_count() const { return (int)boards.size(); }

  void set_max_depth(int d) { max_depth = d; }

  const so::SokobanBoard& board_at(int idx) const { return boards[idx]; }

  // Generate one (state, depth) pair from board boards[idx].
  // Applies between 1 and depth backward moves from the goal state.
  // Returns the achieved depth (may be less than requested if the walk gets
  // stuck).
  s::pair<so::BoardState, int> generate(int board_idx, int depth);

  struct BatchResult {
    s::vector<so::BoardState> states;
    s::vector<int>            board_indices;
    s::vector<int>            costs;
  };

  // Generate a batch: for each of batch_size samples, pick a random board and
  // a random depth in [1, max_depth], then call generate().
  BatchResult next_batch(int batch_size);

private:
  s::vector<so::Dr> shuffled_dirs();
};

#endif // DATA_GENERATOR_H
