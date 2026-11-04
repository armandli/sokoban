#include <game_base.h>
#include <sokoban_state.h>
#include <sokoban_board.h>
#include <sokoban_heuristic.h>

#include <cassert>
#include <filesystem>
#include <format>

#include <utility>
#include <queue>
#include <set>

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
    int va = (*heuristic)(board);
    // heuristic value is larger the better, count is smaller the better
    va -= a.count;
    board.set_state(b.state);
    int vb = (*heuristic)(board);
    vb -= b.count;

    // prioritize bigger values
    return va < vb;
  }
};

// using box move as node instead of player move
int astar(const sf::path& source){
  so::SokobanBoard game(source, true);
  //NodeCmp<decltype(so::heuristic1)> comparator(so::heuristic1, game);
  //NodeCmp<decltype(so::heuristic2)> comparator(so::heuristic2, game);
  //NodeCmp<decltype(so::heuristic3)> comparator(so::heuristic3, game);
  //NodeCmp<decltype(so::heuristic4)> comparator(so::heuristic4, game);
  so::Heuristic5 h5(game);
  NodeCmp<so::Heuristic5> comparator(&h5, game);
  s::priority_queue<Node, s::vector<Node>, decltype(comparator)> q(comparator);
  q.push(Node(game.state(), 0));

  s::set<s::pair<so::BoardState, int>> visited;
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

    for (const so::Pt& box : node.state.boxes){
      for (const so::Dr& dir : so::DIRECTIONS){
        game.set_state(node.state);

        if (not game.is_box_movable(box, dir)){
          continue;
        }

        so::Dr oppo_dir = so::opposite(dir);
        s::optional<s::vector<so::Dr>> steps =
          so::min_player_step(game, box + oppo_dir);
        if (not steps.has_value()){
          continue;
        }

        for (const so::Dr& d : steps.value()){
          game.play(d);
        }
        game.play(dir);

        so::BoardState st = game.state();
        auto key = s::make_pair(st, node.count + (int)steps.value().size() + 1);
        if (visited.find(key) != visited.end()) continue;

        visited.insert(key);
        q.push(Node(st, node.count + (int)steps.value().size() + 1));
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

  sf::path source(argv[1]);
  int steps = astar(source);

  s::cout << "solution in " << steps << s::endl;
}
