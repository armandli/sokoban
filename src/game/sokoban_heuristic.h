#ifndef SOKOBAN_HEURISTIC
#define SOKOBAN_HEURISTIC

#include <game_base.h>
#include <sokoban_board.h>

#include <cassert>
#include <optional>
#include <utility>
#include <queue>

namespace s = std;

namespace sokoban {

int mandist(const Pt& a, const Pt& b){
  return abs(a.x - b.x) + abs(a.y - b.y);
}

bool is_stuck(SokobanBoard& board, const Pt& box){
  bool stuck = true;
  for (const Dr& d : DIRECTIONS){
    Dr od = opposite(d);
    Pt dest = box + d;
    Pt src  = box + od;

    if (board.mSpace.find(dest) != board.mSpace.end() and board.mWalls.find(src) == board.mWalls.end()){
      stuck = false;
      break;
    }
  }
  return stuck;
}

int min_box_step(SokobanBoard& board, const Pt& box){
  assert(board.mState.boxes.find(box) != board.mState.boxes.end());

  if (is_stuck(board, box)) return -1;

  uset<Pt> visited;
  s::queue<s::pair<Pt, int>> q;
  q.push(s::make_pair(box, 0));
  while (not q.empty()){
    auto [loc, count] = q.front(); q.pop();

    if (board.mGoals.find(loc) != board.mGoals.end()) return count;

    for (const Dr& dir : DIRECTIONS){
      Pt new_loc = loc + dir;

      if (visited.find(new_loc) != visited.end()) continue;
      if (board.mSpace.find(new_loc) == board.mSpace.end()) continue;
      if (is_stuck(board, new_loc)) continue;
      if (board.mState.boxes.find(new_loc) != board.mState.boxes.end()) continue;

      visited.insert(new_loc);
      q.push(s::make_pair(new_loc, count+1));
    }
  }

  return -1;
}

// minimum steps player need to take to get to dest
s::optional<s::vector<Dr>> min_player_step(SokobanBoard& board, const Pt& dest){
  assert(board.mSpace.find(dest) != board.mSpace.end());

  uset<Pt> visited;
  s::queue<s::pair<Pt, s::vector<Dr>>> q;
  q.push(s::make_pair(board.mState.player, s::vector<Dr>{}));
  visited.insert(board.mState.player);
  while (not q.empty()){
    auto [loc, actions] = q.front(); q.pop();

    if (loc == dest) return actions; 

    for (const Dr& dir : DIRECTIONS){
      Pt new_loc = loc + dir;

      if (visited.find(new_loc) != visited.end()) continue;
      if (board.mSpace.find(new_loc) == board.mSpace.end()) continue;
      if (board.mState.boxes.find(new_loc) != board.mState.boxes.end()) continue;

      visited.insert(new_loc);

      s::vector<Dr> new_actions = actions;
      new_actions.push_back(dir);
      q.push(s::make_pair(new_loc, new_actions));
    }
  }

  return s::nullopt;
}

uset<Pt> player_reach(SokobanBoard& board){
  uset<Pt> span;
  s::queue<Pt> q;
  q.push(board.mState.player);
  while (not q.empty()){
    Pt pt = q.front(); q.pop();
    span.insert(pt);

    for (const Dr& dir : DIRECTIONS){
      Pt new_pt = pt + dir;
      if (span.find(new_pt) != span.end()) continue;
      if (board.mWalls.find(new_pt) != board.mWalls.end()) continue;
      if (board.mState.boxes.find(new_pt) != board.mState.boxes.end()) continue;
      if (board.mSpace.find(new_pt) == board.mSpace.end()) continue;

      q.push(new_pt);
    }
  }
  return span;
}

// heuristic should return larger value when more favored

// count number of goals reached with boxes
int heuristic1(SokobanBoard& board){
  int sum = 0;
  for (const Pt& box : board.mState.boxes){
    bool on_goal = false;
    for (const Pt& goal : board.mGoals)
      if (box == goal){
        on_goal = true;
        break;
      }
    sum += on_goal;
  }
  return sum; // better when sum is larger
}

// count negative distance between box and its closest goal
int heuristic2(SokobanBoard& board){
  int sum = 0;
  for (const Pt& box : board.mState.boxes){
    int min_dist = 0x7F7F7F7F;
    for (const Pt& goal : board.mGoals){
      int dist = mandist(box, goal);
      min_dist = s::min(min_dist, dist);
    }
    sum += min_dist;
  }
  return sum * -1; // better when sum is smaller
}

// if box stuck, add arbiturary large number,
// if not stuck, add negative manhattan distance from box to closest goal
int heuristic3(SokobanBoard& board){
  int sum = 0;
  for (const Pt& box : board.mState.boxes){
    if (is_stuck(board, box)){
      sum += 0x7F7F7F7F;
    } else {
      int min_dist = 0x7F7F7F7F;
      for (const Pt& goal : board.mGoals){
        int dist = mandist(box, goal);
        min_dist = s::min(min_dist, dist);
      }
      sum += min_dist;
    }
  }
  return sum * -1;
}

// bfs search min cost box distance sum
int heuristic4(SokobanBoard& board){
  int sum = 0;
  for (const Pt& box : board.mState.boxes){
    int min_dist = min_box_step(board, box);
    if (min_dist == -1)
      sum += 0x7F7F7F7F;
    else
      sum += min_dist;
  }
  return sum * -1;
}

} // sokoban

#endif//SOKOBAN_HEURISTIC
