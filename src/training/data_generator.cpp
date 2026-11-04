#include <data_generator.h>

#include <algorithm>
#include <iostream>

DataGenerator::DataGenerator(const sf::path& data_dir, int max_depth, uint32_t seed)
: max_depth(max_depth), rng(seed) {
  for (const auto& entry : sf::recursive_directory_iterator(data_dir)) {
    if (entry.path().extension() != ".bc") continue;
    try {
      boards.emplace_back(entry.path(), /*disable_history=*/true);
    } catch (const s::exception& e) {
      s::cerr << "skip " << entry.path() << ": " << e.what() << "\n";
    }
  }
  s::cerr << "loaded " << boards.size() << " boards from " << data_dir << "\n";
}

s::vector<so::Dr> DataGenerator::shuffled_dirs() {
  s::vector<so::Dr> dirs(so::DIRECTIONS.begin(), so::DIRECTIONS.end());
  s::shuffle(dirs.begin(), dirs.end(), rng);
  return dirs;
}

s::pair<so::BoardState, int> DataGenerator::generate(int board_idx, int depth) {
  so::SokobanBoard& board = boards[board_idx];
  board.set_state(board.goal_state());

  int achieved = 0;
  for (int step = 0; step < depth; ++step) {
    // Collect current boxes into a shuffled vector for random ordering
    s::vector<so::Pt> box_list(board.state().boxes.begin(),
                                board.state().boxes.end());
    s::shuffle(box_list.begin(), box_list.end(), rng);

    bool moved = false;
    for (const so::Pt& box : box_list) {
      for (const so::Dr& dir : shuffled_dirs()) {
        if (board.pull_box(box, dir)) {
          moved = true;
          break;
        }
      }
      if (moved) break;
    }
    if (not moved) break; // stuck — stop walk early
    ++achieved;
  }

  return {board.state(), achieved};
}

DataGenerator::BatchResult
DataGenerator::next_batch(int batch_size) {
  s::uniform_int_distribution<int> board_dist(0, (int)boards.size() - 1);
  s::uniform_int_distribution<int> depth_dist(1, max_depth);

  BatchResult result;
  result.states.reserve(batch_size);
  result.board_indices.reserve(batch_size);
  result.costs.reserve(batch_size);

  for (int i = 0; i < batch_size; ++i) {
    int board_idx = board_dist(rng);
    int depth     = depth_dist(rng);
    auto [st, achieved] = generate(board_idx, depth);
    if (achieved == 0) { --i; continue; } // retry if walk immediately stuck
    result.states.push_back(st);
    result.board_indices.push_back(board_idx);
    result.costs.push_back(achieved);
  }

  return result;
}
