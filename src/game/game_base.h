#ifndef GAME_BASE
#define GAME_BASE

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
  break; case hash(NORTH): return 0;
  break; case hash(SOUTH): return 1;
  break; case hash(EAST):  return 2;
  break; case hash(WEST):  return 3;
  break; default: assert(false);
  }
}

constexpr Dr opposite(const Dr& d){
  switch (hash(d)){
  break; case hash(NORTH): return SOUTH;
  break; case hash(SOUTH): return NORTH;
  break; case hash(EAST):  return WEST;
  break; case hash(WEST):  return EAST;
  break; default: assert(false);
  }
}

s::ostream& operator<<(s::ostream& out, const Dr& d){
  switch (index(d)){
  break; case 0: out << "NORTH";
  break; case 1: out << "SOUTH";
  break; case 2: out << "EAST";
  break; case 3: out << "WEST";
  break; default: assert(false);
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

bool operator==(const Pt& a, const Pt& b){
  return a.x == b.x and a.y == b.y;
}

bool operator!=(const Pt& a, const Pt& b){
  return not operator==(a, b);
}

bool operator<(const Pt& a, const Pt& b){
  if (a.x < b.x)      return true;
  else if (a.x > b.x) return false;
  else if (a.y < b.y) return true;
  else                return false;
}

Pt operator+(const Pt& p, const Dr& d){
  return Pt(p.x+d.x, p.y+d.y);
}

Pt operator+(const Dr& d, const Pt& p){
  return operator+(p, d);
}

s::ostream& operator<<(s::ostream& out, const Pt& pt){
  out << "(" << pt.x << "," << pt.y << ")";
  return out;
}

} // sokoban

namespace std {
template <>
struct hash<sokoban::Pt> {
  auto operator()(const sokoban::Pt& p) const -> size_t {
    return sokoban::hash(p);
  }
};
} // std

#endif//GAME_BASE
