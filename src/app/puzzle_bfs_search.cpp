#include <game_base.h>
#include <sokoban_state.h>
#include <sokoban_board.h>

#include <cassert>
#include <filesystem>
#include <format>

#include <utility>
#include <queue>

#include <iostream>

namespace s = std;
namespace fs = std::filesystem;
namespace g = sokoban;

int bfs(const fs::path& source){
  g::SokobanBoard game(source, true);
  uset<g::BoardState> visited;
  s::queue<s::pair<g::BoardState, int>> q;
  q.push(s::make_pair(game.state(), 0));
  visited.insert(game.state());

  while (not q.empty()){
    auto [state, count] = q.front(); q.pop();

    s::cout << "stepped=" << count << s::endl;

    game.set_state(state);
    if (game.is_terminal()) return count;

    for (g::Dr dr : g::DIRECTIONS){
      game.set_state(state);
      bool diff = game.play(dr);
      if (not diff)
        continue;
      
      g::BoardState st = game.state();
      if (visited.find(st) != visited.end()) continue;

      visited.insert(st);
      q.push(s::make_pair(st, count+1));
    }
  }

  assert(false);
}

int main(int argc, char* argv[]){
  if (argc != 2){
    s::cout << s::format("expect {} <puzzle_file_path>", argv[0]) << s::endl;
    return 1;
  }

  fs::path source(argv[1]);
  int steps = bfs(source);

  s::cout << "solution in " << steps << s::endl;
}
