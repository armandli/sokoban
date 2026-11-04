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

  Node() = default;
  Node(const so::BoardState& st, int c): state(st), count(c) {}
};

template <typename F>
struct NodeCmp {
  F* heuristic;
  so::SokobanBoard& board;
  NodeCmp(F* f, so::SokobanBoard& board): heuristic(f), board(board) {}
  bool operator()(const Node& a, const Node& b){
    board.set_state(a.state);
    int va = heuristic(board);
    // heuristic value is larger the better, count is smaller the better
    va -= a.count;
    board.set_state(b.state);
    int vb = heuristic(board);
    vb -= b.count;

    // prioritize bigger values
    return va < vb;
  }
};

int astar(const sf::path& source){
  so::SokobanBoard game(source, true);
  uset<so::BoardState> visited;
  //NodeCmp<decltype(so::heuristic1)> comparator(so::heuristic1, game);
  //NodeCmp<decltype(so::heuristic2)> comparator(so::heuristic2, game);
  NodeCmp<decltype(so::heuristic3)> comparator(so::heuristic3, game);
  s::priority_queue<Node, s::vector<Node>, decltype(comparator)> q(comparator);
  q.push(Node(game.state(), 0));
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
      q.push(Node(st, node.count+1));
    }
  }

  assert(false);
}

int main(int argc, char* argv[]){
  if (argc != 2){
    s::cout << s::format("expect {} <puzzle_file_path>", argv[0]) << s::endl;
    return 1;
  }

  sf::path source(argv[1]);
  int steps = astar(source);

  s::cout << "solution in " << steps << s::endl;
}
