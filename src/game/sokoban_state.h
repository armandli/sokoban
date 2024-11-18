#ifndef SOKOBAN_STATE
#define SOKOBAN_STATE

#include <type_alias.h>
#include <game_base.h>

namespace s = std;

namespace sokoban {

struct BoardState {
  Pt            player;
  uset<Pt>      boxes;
};

bool operator==(const BoardState& a, const BoardState& b){
  if (a.player != b.player) return false;
  for (const Pt& pt : b.boxes)
    if (a.boxes.find(pt) == a.boxes.end()) return false;
  return true;
}

bool operator!=(const BoardState& a, const BoardState& b){
  return not operator==(a, b);
}

//TODO
//class StateTree {
//public:
//};

} // sokoban

#endif//SOKOBAN_STATE
