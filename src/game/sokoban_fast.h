#ifndef SOKOBAN_FAST_H
#define SOKOBAN_FAST_H

// Bitboard push-level Sokoban environment used for neural heuristic training
// and batched search.
//
//   - Cells are indexed r * FAST_W + c on a fixed 10x10 grid, so a level of
//     up to 10x10 maps 1:1 onto the network input planes.
//   - A state is (boxes, canonical player).  The player is normalised to the
//     smallest cell index of its reachable region, so all states that differ
//     only by where the player walks to are merged.  Edges are box pushes with
//     unit cost, so h(s) estimates remaining pushes.
//   - Pulls are the exact inverse of pushes; states produced by pulling from
//     a solved state are always solvable in <= #pulls pushes.

#include <sokoban_board.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace sokoban {

using u128 = unsigned __int128;

constexpr int FAST_H = 10;
constexpr int FAST_W = 10;
constexpr int FAST_N = FAST_H * FAST_W;

inline constexpr u128 bit(int i){ return (u128)1 << i; }

inline int popcount(u128 x){
  return __builtin_popcountll((uint64_t)x) +
         __builtin_popcountll((uint64_t)(x >> 64));
}

inline int ctz(u128 x){
  uint64_t lo = (uint64_t)x;
  if (lo) return __builtin_ctzll(lo);
  return 64 + __builtin_ctzll((uint64_t)(x >> 64));
}

inline bool test(u128 x, int i){ return (x >> i) & 1; }

// iterate set bits
template <typename F>
inline void for_bits(u128 x, F&& f){
  while (x){
    int i = ctz(x);
    f(i);
    x &= x - 1;
  }
}

struct FastState {
  u128    boxes  = 0;
  int16_t player = 0; // canonical: min index of reachable region
};

inline bool operator==(const FastState& a, const FastState& b){
  return a.boxes == b.boxes and a.player == b.player;
}

inline uint64_t mix64(uint64_t x){
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

struct FastStateHash {
  size_t operator()(const FastState& s) const {
    uint64_t h = mix64((uint64_t)s.boxes);
    h = mix64(h ^ (uint64_t)(s.boxes >> 64));
    return mix64(h ^ (uint64_t)s.player);
  }
};

// direction offsets in the fixed-stride grid: N, S, E, W
constexpr s::array<int, 4> FAST_DR_ROW = {-1, 1, 0,  0};
constexpr s::array<int, 4> FAST_DR_COL = { 0, 0, 1, -1};
constexpr int fast_opposite(int d){ return d ^ 1; }

struct FastLevel {
  s::string name;
  u128 floor = 0;    // walkable cells (inside the level)
  u128 goals = 0;
  u128 dead  = 0;    // floor cells from which a box can never reach a goal
  int  n_boxes = 0;
  FastState start;
  // nb[i][d] = neighbour index or -1 (outside grid / not floor)
  s::array<s::array<int8_t, 4>, FAST_N> nb{};
  // dist[g][i] = min pushes for a lone box at i to reach goals[g] (or INF)
  s::vector<int>               goal_list;
  s::vector<s::array<int16_t, FAST_N>> goal_dist;

  static constexpr int16_t INF = 10000;

  static bool fits(const SokobanBoard& b){
    return b.size().x <= FAST_H and b.size().y <= FAST_W;
  }

  explicit FastLevel(const SokobanBoard& b, s::string level_name = "")
  : name(s::move(level_name)) {
    assert(fits(b));
    for (const Pt& p : b.spaces()) floor |= bit(p.x * FAST_W + p.y);
    for (const Pt& p : b.goals())  goals |= bit(p.x * FAST_W + p.y);
    for (int i = 0; i < FAST_N; ++i)
      for (int d = 0; d < 4; ++d){
        int r = i / FAST_W + FAST_DR_ROW[d];
        int c = i % FAST_W + FAST_DR_COL[d];
        int j = r * FAST_W + c;
        bool ok = r >= 0 and r < FAST_H and c >= 0 and c < FAST_W and
                  test(floor, j) and test(floor, i);
        nb[i][d] = ok ? (int8_t)j : (int8_t)-1;
      }
    BoardState st = b.state();
    u128 boxes = 0;
    for (const Pt& p : st.boxes) boxes |= bit(p.x * FAST_W + p.y);
    n_boxes = (int)st.boxes.size();
    start = canonical(boxes, st.player.x * FAST_W + st.player.y);
    compute_goal_distances();
  }

  // flood fill over free floor from cell `from`
  u128 reach(u128 boxes, int from) const {
    u128 free_ = floor & ~boxes;
    u128 region = bit(from);
    s::array<int, FAST_N> stack;
    int sp = 0;
    stack[sp++] = from;
    while (sp){
      int i = stack[--sp];
      for (int d = 0; d < 4; ++d){
        int j = nb[i][d];
        if (j < 0 or not test(free_, j) or test(region, j)) continue;
        region |= bit(j);
        stack[sp++] = j;
      }
    }
    return region;
  }

  FastState canonical(u128 boxes, int player) const {
    FastState s;
    s.boxes  = boxes;
    s.player = (int16_t)ctz(reach(boxes, player));
    return s;
  }

  bool is_goal(const FastState& s) const { return s.boxes == goals; }

  // 2x2 freeze deadlock touching cell t: all four cells blocked (wall/void or
  // box) and at least one box in the square is off-goal.
  bool square_deadlock(u128 boxes, int t) const {
    int r = t / FAST_W, c = t % FAST_W;
    for (int dr = -1; dr <= 0; ++dr)
      for (int dc = -1; dc <= 0; ++dc){
        int r0 = r + dr, c0 = c + dc;
        bool blocked = true, offgoal = false;
        for (int k = 0; k < 4 and blocked; ++k){
          int rr = r0 + (k >> 1), cc = c0 + (k & 1);
          if (rr < 0 or cc < 0 or rr >= FAST_H or cc >= FAST_W) continue;
          int j = rr * FAST_W + cc;
          if (not test(floor, j)) continue;          // wall / void
          if (test(boxes, j)){
            if (not test(goals, j)) offgoal = true;
            continue;
          }
          blocked = false;
        }
        if (blocked and offgoal) return true;
      }
    return false;
  }

  // Forward push moves.  Pushes into dead squares or 2x2 deadlocks are pruned.
  template <typename F>
  void for_successors(const FastState& s, F&& f) const {
    u128 region = reach(s.boxes, s.player);
    for_bits(s.boxes, [&](int b){
      for (int d = 0; d < 4; ++d){
        int p = nb[b][fast_opposite(d)]; // player stands here
        int t = nb[b][d];                // box goes here
        if (p < 0 or t < 0) continue;
        if (not test(region, p)) continue;
        if (test(s.boxes, t) or test(dead, t)) continue;
        u128 nboxes = (s.boxes & ~bit(b)) | bit(t);
        if (square_deadlock(nboxes, t)) continue;
        f(canonical(nboxes, b));
      }
    });
  }

  s::vector<FastState> successors(const FastState& s) const {
    s::vector<FastState> out;
    for_successors(s, [&](const FastState& n){ out.push_back(n); });
    return out;
  }

  // Reverse moves (pulls).  Player at b+d steps to b+2d dragging box to b+d.
  template <typename F>
  void for_predecessors(const FastState& s, F&& f) const {
    u128 region = reach(s.boxes, s.player);
    for_bits(s.boxes, [&](int b){
      for (int d = 0; d < 4; ++d){
        int p = nb[b][d];
        if (p < 0 or not test(region, p)) continue;
        int q = nb[p][d];
        if (q < 0 or test(s.boxes, q)) continue;
        u128 nboxes = (s.boxes & ~bit(b)) | bit(p);
        f(canonical(nboxes, q));
      }
    });
  }

  // All distinct canonical solved states (one per player region).
  s::vector<FastState> goal_states() const {
    s::vector<FastState> out;
    u128 left = floor & ~goals;
    while (left){
      int p = ctz(left);
      FastState s = canonical(goals, p);
      out.push_back(s);
      left &= ~reach(goals, p);
    }
    return out;
  }

  // Lower-bound heuristic: min-cost box->goal matching on lone-box push
  // distances (same idea as Heuristic5).  Returns INF on deadlock.
  int matching_cost(const FastState& s) const {
    int n = (int)goal_list.size();
    int boxes[16];
    int k = 0;
    for_bits(s.boxes, [&](int b){ if (k < 16) boxes[k++] = b; });
    if (k != n or n > 8) return INF;
    int perm[16];
    s::iota(perm, perm + n, 0);
    int best = INF;
    do {
      int total = 0;
      for (int i = 0; i < n and total < best; ++i)
        total += goal_dist[perm[i]][boxes[i]];
      best = s::min(best, total);
    } while (s::next_permutation(perm, perm + n));
    return best >= INF ? INF : best;
  }

private:
  void compute_goal_distances(){
    for_bits(goals, [&](int g){ goal_list.push_back(g); });
    goal_dist.assign(goal_list.size(), {});
    u128 live = 0;
    for (size_t gi = 0; gi < goal_list.size(); ++gi){
      auto& dist = goal_dist[gi];
      dist.fill(INF);
      s::queue<int> q;
      dist[goal_list[gi]] = 0;
      q.push(goal_list[gi]);
      while (not q.empty()){
        int c = q.front(); q.pop();
        live |= bit(c);
        // a box at c2 = c+d can be pushed to c by a player standing at c+2d
        for (int d = 0; d < 4; ++d){
          int c2 = nb[c][d];
          if (c2 < 0) continue;
          int c3 = nb[c2][d];
          if (c3 < 0 or dist[c2] != INF) continue;
          dist[c2] = dist[c] + 1;
          q.push(c2);
        }
      }
    }
    dead = floor & ~live;
  }
};

// Random backward walk: start from a random solved state, apply up to
// `depth` random pulls.  Returns the state and the number of pulls applied
// (an upper bound on the true push distance).
template <typename RNG>
s::pair<FastState, int> random_pull_walk(const FastLevel& L, int depth,
                                          RNG& rng){
  // random player cell outside the boxes
  u128 free_ = L.floor & ~L.goals;
  int nfree = popcount(free_);
  int pick = s::uniform_int_distribution<int>(0, nfree - 1)(rng);
  int player = -1;
  for_bits(free_, [&](int i){ if (pick-- == 0) player = i; });
  FastState s = L.canonical(L.goals, player);

  // Randomised DFS with backtracking.  A plain random walk dead-ends ~90% of
  // the time within a few pulls (the player pulls a box into a corridor and
  // is trapped behind it), starving training of deep states.  On a dead end
  // we undo the last pull and try another, within an expansion budget; the
  // deepest state found is returned.
  struct Frame { FastState st; s::vector<FastState> cand; };
  s::vector<Frame> stack;
  auto push_frame = [&](const FastState& x){
    Frame f{x, {}};
    L.for_predecessors(x, [&](const FastState& p){ f.cand.push_back(p); });
    s::shuffle(f.cand.begin(), f.cand.end(), rng);
    stack.push_back(s::move(f));
  };
  push_frame(s);
  FastState deepest = s;
  int best = 0;
  long budget = 8L * depth + 64;
  while (not stack.empty() and (int)stack.size() - 1 < depth and budget-- > 0){
    Frame& top = stack.back();
    if (top.cand.empty()){ stack.pop_back(); continue; }
    FastState nxt = top.cand.back();
    top.cand.pop_back();
    push_frame(nxt);
    if ((int)stack.size() - 1 > best){ best = (int)stack.size() - 1; deepest = nxt; }
  }
  return {deepest, best};
}

// Exact push distances for every state within `max_depth` pulls of a solved
// state (backward BFS from all goal states).  Stops early at a complete layer
// once `max_states` is exceeded so all returned distances stay exact.
inline s::vector<s::pair<FastState, int>>
exact_distance_layers(const FastLevel& L, int max_depth, size_t max_states){
  s::unordered_map<FastState, int, FastStateHash> dist;
  s::vector<FastState> frontier = L.goal_states();
  for (auto& g : frontier) dist[g] = 0;
  for (int d = 1; d <= max_depth and dist.size() < max_states; ++d){
    s::vector<FastState> next;
    for (const FastState& s : frontier)
      L.for_predecessors(s, [&](const FastState& p){
        if (dist.emplace(p, d).second) next.push_back(p);
      });
    if (next.empty()) break;
    frontier.swap(next);
  }
  s::vector<s::pair<FastState, int>> out(dist.begin(), dist.end());
  return out;
}

} // namespace sokoban

#endif // SOKOBAN_FAST_H
