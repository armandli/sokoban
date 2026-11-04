#ifndef GAME_BASE_H
#define GAME_BASE_H

#include <type_alias.h>

#include <cassert>
#include <array>
#include <ostream>

namespace s = std;

namespace sokoban {

struct Dr {
  sint x;
  sint y;
  Dr() = default;
  constexpr Dr(sint x, sint y): x(x), y(y) {}
};

constexpr size_t hash(const Dr& d){
  return (((ull)d.x) << 8U) + (ull)d.y;
}

constexpr bool operator==(const Dr& a, const Dr& b){
  return a.x == b.x and a.y == b.y;
}

constexpr bool operator!=(const Dr& a, const Dr& b){
  return not operator==(a, b);
}

constexpr Dr NORTH(-1, 0);
constexpr Dr SOUTH( 1, 0);
constexpr Dr EAST(  0, 1);
constexpr Dr WEST(  0,-1);

constexpr s::array<Dr, 4> DIRECTIONS = {
  NORTH,
  SOUTH,
  EAST,
  WEST,
};

constexpr uint16_t index(const Dr& d){
  switch (hash(d)){
  case hash(NORTH): return 0;
  break; case hash(SOUTH): return 1;
  break; case hash(EAST):  return 2;
  break; case hash(WEST):  return 3;
  default: assert(false);
  }
}

constexpr Dr opposite(const Dr& d){
  switch (hash(d)){
  case hash(NORTH): return SOUTH;
  break; case hash(SOUTH): return NORTH;
  break; case hash(EAST):  return WEST;
  break; case hash(WEST):  return EAST;
  default: assert(false);
  }
}

inline s::ostream& operator<<(s::ostream& out, const Dr& d){
  switch (index(d)){
  case 0: out << "NORTH";
  break; case 1: out << "SOUTH";
  break; case 2: out << "EAST";
  break; case 3: out << "WEST";
  default: assert(false);
  }
  return out;
}

struct Pt {
  sint x;
  sint y;
  Pt() = default;
  Pt(sint x, sint y): x(x), y(y) {}

  Pt& operator+=(const Dr& d){
    x += d.x;
    y += d.y;
    return *this;
  }
};

constexpr size_t hash(const Pt& p){
  return ((ull)p.x << 16U) + (ull)p.y;
}

inline bool operator==(const Pt& a, const Pt& b){
  return a.x == b.x and a.y == b.y;
}

inline bool operator!=(const Pt& a, const Pt& b){
  return not operator==(a, b);
}

inline bool operator<(const Pt& a, const Pt& b){
  if (a.x < b.x)      return true;
  else if (a.x > b.x) return false;
  else if (a.y < b.y) return true;
  else                return false;
}

inline Pt operator+(const Pt& p, const Dr& d){
  return Pt(p.x+d.x, p.y+d.y);
}

inline Pt operator+(const Dr& d, const Pt& p){
  return operator+(p, d);
}

inline s::ostream& operator<<(s::ostream& out, const Pt& pt){
  out << "(" << pt.x << "," << pt.y << ")";
  return out;
}

} // namespace sokoban

namespace std {
template <>
struct hash<sokoban::Pt> {
  auto operator()(const sokoban::Pt& p) const -> size_t {
    return sokoban::hash(p);
  }
};
} // namespace std

#endif // GAME_BASE_H
