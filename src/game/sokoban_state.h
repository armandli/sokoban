#ifndef SOKOBAN_STATE_H
#define SOKOBAN_STATE_H

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
  if (a.boxes.size() != b.boxes.size()) return false;
  for (const Pt& pt : b.boxes)
    if (a.boxes.find(pt) == a.boxes.end()) return false;
  return true;
}

bool operator!=(const BoardState& a, const BoardState& b){
  return not operator==(a, b);
}

bool operator<(const BoardState& a, const BoardState& b){
  if (a.player < b.player)      return true;
  else if (b.player < a.player) return false;

  if (a.boxes.size() < b.boxes.size())      return true;
  else if (a.boxes.size() > b.boxes.size()) return false;

  for (auto itera = s::begin(a.boxes), iterb = s::begin(b.boxes);
       itera != s::end(a.boxes) and iterb != s::end(b.boxes);
       ++itera, ++iterb){
    if (*itera < *iterb)      return true;
    else if (*iterb < *itera) return false;
  }
  return false;
}

size_t fnv1(const BoardState& st){
  uint32_t prime = 0x01000193;
  uint32_t h = 0x811c9dc5;
  s::hash<Pt> hasher;
  h ^= hasher(st.player);
  h *= prime;
  uint32_t box_hash = 0;
  for (const Pt& box : st.boxes)
    box_hash ^= hasher(box);
  h ^= box_hash;
  h *= prime;
  return h;
}

} // namespace sokoban

namespace std {
template <>
struct hash<sokoban::BoardState> {
  auto operator()(const sokoban::BoardState& st) const -> size_t {
    return sokoban::fnv1(st);
  }
};
} // namespace std

#endif // SOKOBAN_STATE_H
