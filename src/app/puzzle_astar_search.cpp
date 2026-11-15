#include <game_base.h>
#include <sokoban_state.h>
#include <sokoban_board.h>
#include <sokoban_heuristic.h>

#include <cassert>
#include <filesystem>
#include <format>

#include <utility>
#include <queue>

#include <iostream>

namespace s  = std;
namespace sf = std::filesystem;
namespace so = sokoban;

struct Node {
  so::BoardState state;
  int            count;
  int            value; // h(state) - count, larger is better

  Node() = default;
  Node(const so::BoardState& st, int c, int v): state(st), count(c), value(v) {}
};

template <so::Heuristic H>
int astar(so::SokobanBoard& game, H& h){
  uset<so::BoardState> visited;

  // heuristic is evaluated once per node, not on every heap comparison
  auto cmp = [](const Node& a, const Node& b){ return a.value < b.value; };

  s::priority_queue<Node, s::vector<Node>, decltype(cmp)> q(cmp);
  q.push(Node(game.state(), 0, h(game)));
  visited.insert(game.state());

  int c = 0;

  while (not q.empty()){
    Node node = q.top(); q.pop();
    c++;

    s::cout << "stepped=" << node.count << s::endl;

    game.set_state(node.state);
    game.print(s::cout);
    s::cout << s::flush;

    if (game.is_terminal()){
      s::cout << "explored " << c << " nodes" << s::endl;
      return node.count;
    }

    for (so::Dr dr : so::DIRECTIONS){
      game.set_state(node.state);
      bool diff = game.play(dr);
      if (not diff) continue;

      so::BoardState st = game.state();
      if (visited.find(st) != visited.end()) continue;

      visited.insert(st);
      q.push(Node(st, node.count + 1, h(game) - (node.count + 1)));
    }
  }

  return -1; // unsolvable (falling off a non-void function is UB under NDEBUG)
}

int main(int argc, char* argv[]){
  if (argc != 2){
    s::cout << s::format("expect {} <puzzle_file_path>", argv[0]) << s::endl;
    return 1;
  }

  sf::path source(argv[1]);
  so::SokobanBoard game(source, true);

  //int steps = astar(game, so::heuristic1);
  //int steps = astar(game, so::heuristic2);
  int steps = astar(game, so::heuristic3);

  s::cout << "solution in " << steps << s::endl;
}
