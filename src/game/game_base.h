#ifndef GAME_BASE
#define GAME_BASE

#include <type_alias.h>

#include <array>

namespace s = std;

namespace sokoban {

struct Dr {
  sint x;
  sint y;
  Dr() = default;
  constexpr Dr(sint x, sint y): x(x), y(y) {}
};

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

template <Dr dr> constexpr uint16_t index();
template <>      constexpr uint16_t index<NORTH>(){ return 0; }
template <>      constexpr uint16_t index<SOUTH>(){ return 1; }
template <>      constexpr uint16_t index<EAST>(){  return 2; }
template <>      constexpr uint16_t index<WEST>(){  return 3; }

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

} // sokoban

namespace std {
template <>
struct hash<sokoban::Pt> {
  auto operator()(const sokoban::Pt& p) const -> size_t {
    return (((ull)p.x) << 16U) + (ull)p.y;
  }
};
} // std

#endif//GAME_BASE
