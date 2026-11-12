#ifndef BATCH_SEARCH_H
#define BATCH_SEARCH_H

// Batched weighted A* (DeepCubeA-style BWAS) over the push graph.
// Each iteration pops up to `batch` nodes, expands them, and evaluates all new
// children with one heuristic call, so a neural heuristic runs on the GPU in
// large batches.  Priority f = g + weight * h.

#include <sokoban_fast.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace s  = std;
namespace so = sokoban;

struct SearchConfig {
  double weight       = 1.0;
  int    batch        = 32;
  long   max_expanded = 5000;
};

struct SearchResult {
  bool   solved   = false;
  int    pushes   = -1;
  long   expanded = 0;
  long   generated = 0;
  double seconds  = 0;
  s::vector<so::FastState> path; // start .. goal
};

// Eval: (level, states) -> heuristic per state (remaining pushes estimate)
using BatchEval = s::function<s::vector<float>(const so::FastLevel&,
                                               const s::vector<so::FastState>&)>;

inline SearchResult batch_wastar(const so::FastLevel& L, const BatchEval& eval,
                                 const SearchConfig& cfg){
  auto t0 = s::chrono::steady_clock::now();
  SearchResult res;

  struct NodeRec { so::FastState st; int g; int parent; };
  s::vector<NodeRec> nodes;
  s::unordered_map<so::FastState, int, so::FastStateHash> index; // -> node id
  using QE = s::pair<float, int>; // (f, node id)
  s::priority_queue<QE, s::vector<QE>, s::greater<QE>> open;

  auto finish = [&](int id){
    res.solved = true;
    res.pushes = nodes[id].g;
    for (int i = id; i >= 0; i = nodes[i].parent) res.path.push_back(nodes[i].st);
    s::reverse(res.path.begin(), res.path.end());
  };

  nodes.push_back({L.start, 0, -1});
  index[L.start] = 0;
  if (L.is_goal(L.start)){ finish(0); return res; }
  open.push({0.f, 0});

  s::vector<int> popped;
  s::vector<so::FastState> child_states;
  s::vector<int> child_ids;
  while (not open.empty() and res.expanded < cfg.max_expanded){
    popped.clear();
    while (not open.empty() and (int)popped.size() < cfg.batch){
      auto [f, id] = open.top(); open.pop();
      (void)f;
      if (index[nodes[id].st] != id) continue; // superseded by a better g
      popped.push_back(id);
    }
    child_states.clear();
    child_ids.clear();
    int goal_id = -1;
    for (int id : popped){
      ++res.expanded;
      so::FastState cur = nodes[id].st;
      int g = nodes[id].g + 1;
      L.for_successors(cur, [&](const so::FastState& n){
        ++res.generated;
        auto it = index.find(n);
        if (it != index.end() and nodes[it->second].g <= g) return;
        int nid = (int)nodes.size();
        nodes.push_back({n, g, id});
        index[n] = nid;
        if (L.is_goal(n)){
          if (goal_id < 0 or g < nodes[goal_id].g) goal_id = nid;
          return;
        }
        child_states.push_back(n);
        child_ids.push_back(nid);
      });
    }
    if (goal_id >= 0){ finish(goal_id); break; }
    if (child_states.empty()) continue;
    s::vector<float> h = eval(L, child_states);
    for (size_t i = 0; i < child_ids.size(); ++i){
      float hv = s::max(0.f, h[i]);
      int id = child_ids[i];
      open.push({(float)(nodes[id].g + cfg.weight * hv), id});
    }
  }
  res.seconds = s::chrono::duration<double>(
      s::chrono::steady_clock::now() - t0).count();
  return res;
}

// Classical baseline: lone-box matching lower bound (INF => deadlock).
inline s::vector<float> matching_eval(const so::FastLevel& L,
                                      const s::vector<so::FastState>& states){
  s::vector<float> out(states.size());
  for (size_t i = 0; i < states.size(); ++i){
    int c = L.matching_cost(states[i]);
    out[i] = c >= so::FastLevel::INF ? 1e6f : (float)c;
  }
  return out;
}

#endif // BATCH_SEARCH_H
