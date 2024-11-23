#include <game_base.h>
#include <sokoban_state.h>
#include <sokoban_board.h>
#include <sokoban_heuristic.h>

#include <cassert>
#include <filesystem>
#include <format>

#include <vector>
#include <queue>
#include <set>

#include <iostream>

namespace s = std;
namespace fs = std::filesystem;
namespace g = sokoban;

struct Node {
  g::BoardState state;
  int           count;

  Node() = default;
  Node(const g::BoardState& st, int c): state(st), count(c) {}
};

auto NodeCmp = [](const Node& a, const Node& b){
  // prioritize smaller values
  return a.count > b.count;
};

// using box move as node instead of player move
int bfs(const fs::path& source){
  g::SokobanBoard game(source, true);
  s::priority_queue<Node, s::vector<Node>, decltype(NodeCmp)> q(NodeCmp);
  q.push(Node(game.state(), 0));

  s::set<s::pair<g::BoardState, int>> visited;
  visited.insert(s::make_pair(game.state(), 0));

  int c = 0;

  while (not q.empty()){
    Node node = q.top(); q.pop();

    c++;

    s::cout << "stepped=" << node.count << s::endl;

    game.set_state(node.state);
    game.print(s::cout); s::cout << s::flush;

    if (game.is_terminal()){
      s::cout << "explored " << c << " nodes" << s::endl;
      return node.count;
    }

    for (const g::Pt& box : node.state.boxes){
      for (const g::Dr& dir : g::DIRECTIONS){
        game.set_state(node.state);

        if (not game.is_box_movable(box, dir)){
          continue;
        }

        g::Dr oppo_dir = g::opposite(dir);
        s::optional<s::vector<g::Dr>> steps = g::min_player_step(game, box + oppo_dir);
        if (not steps.has_value()){
          continue;
        }

        for (const g::Dr& d : steps.value()){
          game.play(d);
        }
        game.play(dir);

        g::BoardState st = game.state();
        if (visited.find(s::make_pair(st, node.count+steps.value().size()+1)) != visited.end()){
          continue;
        }

        visited.insert(s::make_pair(st, node.count+steps.value().size()+1));

        q.push(Node(st, node.count + steps.value().size() + 1));
      }
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
