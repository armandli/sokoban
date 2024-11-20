#ifndef SOKOBAN_HEURISTIC
#define SOKOBAN_HEURISTIC

#include <game_base.h>
#include <sokoban_board.h>

namespace sokoban {

int mandist(const Pt& a, const Pt& b){
  return abs(a.x - b.x) + abs(a.y - b.y);
}

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
  return sum * -1;
}

//TODO: incorrect 
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
  return sum * -1;
}

} // sokoban

#endif//SOKOBAN_HEURISTIC
