#!/usr/bin/env python3
"""Run neural-heuristic training experiments and final evaluations.

Experiments are defined in scripts/experiments.py as named stages.  Each run
writes to runs/<name>/ (metrics.jsonl, latest.pt, best.pt, train.log) and,
once training finishes, final evaluations on the full held-out test split to
runs/<name>/eval_b<budget>.json.  Completed runs are skipped, so the sweep can
be restarted at any time.

  python3 scripts/sweep.py --stage screen            # run a stage
  python3 scripts/sweep.py --stage screen --parallel 2
  python3 scripts/sweep.py --list
"""
import argparse
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import experiments  # noqa: E402

TRAIN = ROOT / "build/bin/puzzle_train"
SOLVE = ROOT / "build/bin/puzzle_astar_nn"
RUNS = ROOT / "runs"
EVAL_BUDGETS = (300, 1000)


def is_done(run_dir: Path) -> bool:
    m = run_dir / "metrics.jsonl"
    if not m.exists():
        return False
    for line in m.read_text().splitlines():
        try:
            d = json.loads(line)
        except json.JSONDecodeError:
            continue
        if d.get("event") == "done" and not d.get("interrupted"):
            return True
    return False


def final_eval(run_dir: Path, weights: Path, tag: str = "") -> None:
    for b in EVAL_BUDGETS:
        out = run_dir / f"eval{tag}_b{b}.json"
        if out.exists():
            continue
        cmd = [str(SOLVE), "--weights", str(weights), "--eval-dir", "data/boxoban",
               "--split", "test", "--budget", str(b), "--batch", "16", "--amp", "1",
               "--json", str(out)]
        with open(run_dir / "eval.log", "a") as log:
            subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=False)


def baseline_eval() -> None:
    d = RUNS / "_baseline_matching"
    d.mkdir(parents=True, exist_ok=True)
    for b in EVAL_BUDGETS:
        out = d / f"eval_b{b}.json"
        if out.exists():
            continue
        subprocess.run([str(SOLVE), "--heuristic", "matching", "--eval-dir", "data/boxoban",
                        "--split", "test", "--budget", str(b), "--batch", "16",
                        "--json", str(out)], cwd=ROOT, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=False)


def run_one(name: str, args: list, threads: int) -> str:
    run_dir = RUNS / name
    if is_done(run_dir):
        final_eval(run_dir, run_dir / "best.pt")
        return f"{name}: already done"
    run_dir.mkdir(parents=True, exist_ok=True)
    # a previous interrupted attempt leaves partial metrics; start fresh
    for f in ("metrics.jsonl",):
        p = run_dir / f
        if p.exists():
            p.rename(run_dir / f"{f}.interrupted.{int(time.time())}")
    cmd = [str(TRAIN), "--out", str(run_dir), "--threads", str(threads), "--amp", "1"] + \
        [str(a) for a in args]
    t0 = time.time()
    with open(run_dir / "train.log", "a") as log:
        log.write("$ " + " ".join(cmd) + "\n")
        log.flush()
        rc = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT).returncode
    if rc != 0:
        return f"{name}: FAILED rc={rc} (see {run_dir/'train.log'})"
    if (run_dir / "best.pt").exists():
        final_eval(run_dir, run_dir / "best.pt")
    return f"{name}: finished in {(time.time()-t0)/60:.1f} min"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", action="append", help="stage name(s) to run")
    ap.add_argument("--parallel", type=int, default=1)
    ap.add_argument("--threads", type=int, default=0, help="CPU threads per run")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    if a.list:
        for st, exps in experiments.STAGES.items():
            print(f"[{st}]")
            for n, args in exps:
                print(f"  {n:32s} {'done' if is_done(RUNS/n) else '':5s} {' '.join(map(str,args))}")
        return
    if not a.stage:
        ap.error("--stage required")
    threads = a.threads or max(4, (os.cpu_count() or 8) // a.parallel - 2)
    baseline_eval()
    for st in a.stage:
        exps = experiments.STAGES[st]
        print(f"== stage {st}: {len(exps)} runs, parallel={a.parallel}", flush=True)
        with ThreadPoolExecutor(max_workers=a.parallel) as ex:
            for msg in ex.map(lambda e: run_one(e[0], e[1], threads), exps):
                print(msg, flush=True)


if __name__ == "__main__":
    main()
