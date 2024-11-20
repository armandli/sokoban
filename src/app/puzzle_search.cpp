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

namespace s = std;
namespace fs = std::filesystem;
namespace g = sokoban;

struct Node {
  g::BoardState state;
  int           count;

  Node() = default;
  Node(const g::BoardState& st, int c): state(st), count(c) {}
};

template <typename F>
struct NodeCmp {
  F* heuristic;
  g::SokobanBoard& board;
  NodeCmp(F* f, g::SokobanBoard& board): heuristic(f), board(board) {}
  bool operator()(const Node& a, const Node& b){
    board.set_state(a.state);
    int va = heuristic(board);
    //va += a.count;
    board.set_state(b.state);
    int vb = heuristic(board);
    //vb += b.count;
    return va > vb;
  }
};

int astar(const fs::path& source){
  g::SokobanBoard game(source, true);
  uset<g::BoardState> visited;
  NodeCmp<decltype(g::heuristic1)> comparator(g::heuristic1, game);
  //NodeCmp<decltype(g::heuristic2)> comparator(g::heuristic2, game);
  s::priority_queue<Node, s::vector<Node>, decltype(comparator)> q(comparator);
  q.push(Node(game.state(), 0));
  visited.insert(game.state());

  int c = 0;

  while (not q.empty()){
    Node node = q.top(); q.pop();
    c++;

    s::cout << "stepped=" << node.count << s::endl;

    game.set_state(node.state);
    //game.print(s::cout);
    //s::cout << s::flush;

    if (game.is_terminal()){
      s::cout << "explored " << c << " nodes" << s::endl;
      return node.count;
    }

    for (g::Dr dr : g::DIRECTIONS){
      game.set_state(node.state);
      bool diff = game.play(dr);
      if (not diff) continue;

      g::BoardState st = game.state();
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

  fs::path source(argv[1]);
  int steps = astar(source);

  s::cout << "solution in " << steps << s::endl;
}
