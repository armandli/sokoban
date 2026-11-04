#ifndef TYPE_ALIAS_H
#define TYPE_ALIAS_H

#include <cstdint>
#include <unordered_set>

namespace s = std;

using sint = int16_t;
using ull  = uint64_t;

template <typename T> using uset = s::unordered_set<T>;

#endif // TYPE_ALIAS_H
