# sokoban
sokoban game implementation and a star algorithm search, with a neural network
heuristic trained by approximate value iteration (DeepCubeA-style).

## Build

```sh
sh bin/rebuild.sh     # clean configure + build (picks newest g++, Release)
sh bin/build.sh       # incremental build
```

Requires libtorch (searched at `~/.local/libtorch`, then
`~/alphago_zero/libtorch`; override with `-DTORCH_PATH=...`) and, for CUDA
builds of libtorch, `nvcc` (looked up under `$CUDA_HOME` or `/usr/local/cuda`).
FTXUI and CLI11 are fetched by CMake.

## Programs (build/bin)

| program | purpose |
|---|---|
| `puzzle_play <file.bc>` | interactive terminal play |
| `puzzle_bfs_search`, `puzzle_astar_search` | player-move search |
| `puzzle_bfs_search2`, `puzzle_astar_search2` | box-move search (Heuristic5) |
| `puzzle_train` | train a neural push-distance heuristic |
| `puzzle_astar_nn` | batched weighted A* with a trained heuristic, single puzzles or split evaluation |

## Neural heuristic

* **Environment** (`src/game/sokoban_fast.h`): bitboard push-level model on a
  10x10 grid. States are (boxes, canonical player region); edges are pushes
  (unit cost). Pushes into dead squares and 2x2 freeze deadlocks are pruned.
  Cross-checked against `SokobanBoard`.
* **Data**: random backward (pull) walks with backtracking from solved states
  of training levels; walk length is an upper bound on push distance.
* **Targets**: `y = max(matching_lb, min(walk_len, 1 + min_succ h_frozen(s')))`
  — Bellman backup with the network frozen at the start of each iteration,
  clamped below by the admissible lone-box matching bound.
* **Models** (`src/model`): `cnn` (plain conv stack), `resnet` (residual tower,
  options `--head flat|gap`, `--se` global-pooling context, `--dilate`), `attn`
  (pre-norm transformer over cells + CLS token).
* **Split**: levels under `data/boxoban` are split by filename hash; ~10% held
  out (`is_test_level` in `src/training/level_set.h`).
* **Validation**: exact push distances from backward BFS on held-out levels
  (`val_mae`), and solve rate of held-out puzzle starts under an expansion
  budget (`solve_rate`).

### Training and monitoring

```sh
# single run
build/bin/puzzle_train --out runs/my_run --arch resnet --channels 64 --blocks 6 --amp 1

# experiment stages (defined in scripts/experiments.py), restartable
python3 scripts/sweep.py --list
python3 scripts/sweep.py --stage screen --parallel 2

# monitoring: status table, live refresh, HTML report with learning curves
python3 scripts/monitor.py
python3 scripts/monitor.py --watch 60
python3 scripts/monitor.py --html runs/report.html

# evaluate / solve
build/bin/puzzle_astar_nn --weights runs/my_run/best.pt --eval-dir data/boxoban --split test --budget 1000
build/bin/puzzle_astar_nn --weights runs/my_run/best.pt data/boxoban/hard/bx0001.bc --show
build/bin/puzzle_astar_nn --heuristic matching --eval-dir data/boxoban --split test
```

Each run directory holds `args.txt`, `model.cfg`, `metrics.jsonl` (one JSON
object per iteration), `latest.pt`, `best.pt` (by solve rate) and, after a
sweep, `eval_b300.json` / `eval_b1000.json` on the full test split.
