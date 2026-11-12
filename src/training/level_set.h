#ifndef LEVEL_SET_H
#define LEVEL_SET_H

#include <sokoban_board.h>
#include <sokoban_fast.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace s  = std;
namespace sf = std::filesystem;
namespace so = sokoban;

// Deterministic split by FNV-1a of the file stem: 1 in `test_mod` levels is
// held out for evaluation.  Stable across platforms and runs.
inline bool is_test_level(const s::string& stem, int test_mod = 10){
  uint32_t h = 0x811c9dc5u;
  for (unsigned char c : stem){ h ^= c; h *= 0x01000193u; }
  return h % (uint32_t)test_mod == 0;
}

struct LevelSet {
  s::vector<so::FastLevel> train;
  s::vector<so::FastLevel> test;
};

// Load every .bc under `dir` that fits the 10x10 network grid.
inline LevelSet load_levels(const sf::path& dir, int test_mod = 10){
  s::vector<sf::path> files;
  for (const auto& e : sf::recursive_directory_iterator(dir))
    if (e.path().extension() == ".bc") files.push_back(e.path());
  s::sort(files.begin(), files.end());

  LevelSet ls;
  int skipped = 0;
  for (const auto& f : files){
    try {
      so::SokobanBoard b(f, /*disable_history=*/true);
      if (not so::FastLevel::fits(b) or b.goals().size() != b.state().boxes.size() or
          b.goals().empty()){
        ++skipped;
        continue;
      }
      so::FastLevel L(b, f.stem().string());
      (is_test_level(L.name, test_mod) ? ls.test : ls.train).push_back(s::move(L));
    } catch (const s::exception& e){
      s::cerr << "skip " << f << ": " << e.what() << "\n";
      ++skipped;
    }
  }
  s::cerr << "levels: " << ls.train.size() << " train, " << ls.test.size()
          << " test, " << skipped << " skipped (from " << dir << ")\n";
  return ls;
}

#endif // LEVEL_SET_H
