#!/usr/bin/env python3
"""Monitor neural-heuristic training runs.

  python3 scripts/monitor.py                 # status table of all runs
  python3 scripts/monitor.py --watch 60      # refresh every 60 s
  python3 scripts/monitor.py --html runs/report.html   # SVG learning curves

Reads runs/*/metrics.jsonl (written by puzzle_train) and runs/*/eval_b*.json
(written by sweep.py final evaluations on the full held-out test split).
"""
import argparse
import html
import json
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNS = ROOT / "runs"


def load_run(d: Path) -> dict:
    r = {"name": d.name, "iters": [], "start": {}, "baseline": {}, "baseline_val": {},
         "done": None, "evals": {}, "args": ""}
    m = d / "metrics.jsonl"
    if m.exists():
        for line in m.read_text().splitlines():
            try:
                x = json.loads(line)
            except json.JSONDecodeError:
                continue
            ev = x.get("event")
            if ev == "iter":
                r["iters"].append(x)
            elif ev in ("start", "baseline", "baseline_val"):
                r[ev] = x
            elif ev == "done":
                r["done"] = x
        r["mtime"] = m.stat().st_mtime
    for f in d.glob("eval*_b*.json"):
        try:
            r["evals"][f.stem] = json.loads(f.read_text())
        except json.JSONDecodeError:
            pass
    a = d / "args.txt"
    if a.exists():
        r["args"] = a.read_text().strip()
    return r


def load_all() -> list:
    if not RUNS.exists():
        return []
    return [load_run(d) for d in sorted(RUNS.iterdir()) if d.is_dir()]


def status(r: dict) -> str:
    if r["done"]:
        return "stopped" if r["done"].get("interrupted") else "done"
    if not r["iters"] and not r["start"]:
        return "-"
    age = time.time() - r.get("mtime", 0)
    return "running" if age < 600 else f"stale {age/3600:.1f}h"


def best(vals):
    vals = [v for v in vals if v is not None]
    return (min(vals) if vals else None)


def fmt(v, p=3):
    if v is None:
        return "-"
    if isinstance(v, float):
        return f"{v:.{p}f}"
    return str(v)


def table(runs: list) -> str:
    hdr = ["run", "status", "params", "iter", "min", "loss", "val_mae", "best_mae",
           "solve@1k(128)", "best", "test@300", "test@1k", "exp@1k"]
    rows = []
    for r in runs:
        it = r["iters"][-1] if r["iters"] else {}
        solves = [x.get("solve_rate") for x in r["iters"] if "solve_rate" in x]
        maes = [x.get("val_mae") for x in r["iters"]]
        e300 = r["evals"].get("eval_b300", {})
        e1k = r["evals"].get("eval_b1000", {})
        rows.append([
            r["name"], status(r), fmt(r["start"].get("params")), fmt(it.get("iter")),
            fmt((it.get("time") or 0) / 60, 1) if it else "-",
            fmt(it.get("loss"), 2), fmt(it.get("val_mae")), fmt(best(maes)),
            fmt(solves[-1]) if solves else "-", fmt(max(solves)) if solves else "-",
            fmt(e300.get("solve_rate")), fmt(e1k.get("solve_rate")),
            fmt(e1k.get("mean_expanded"), 0),
        ])
    w = [max(len(str(x)) for x in col) for col in zip(hdr, *rows)] if rows else [len(h) for h in hdr]
    line = lambda xs: "  ".join(str(x).ljust(n) for x, n in zip(xs, w))
    out = [line(hdr), line(["-" * n for n in w])] + [line(x) for x in rows]
    base = next((r for r in runs if r["baseline_val"]), None)
    if base:
        out.append(f"\nmatching baseline: val_mae {fmt(base['baseline_val'].get('val_mae'))}"
                   f", solve@1k(128) {fmt(base['baseline'].get('solve_rate'))}")
    return "\n".join(out)


def gpu_line() -> str:
    try:
        q = subprocess.run(["nvidia-smi", "--query-gpu=utilization.gpu,memory.used,memory.total,temperature.gpu",
                            "--format=csv,noheader,nounits"], capture_output=True, text=True, timeout=10)
        u, mu, mt, t = [x.strip() for x in q.stdout.strip().split(",")]
        return f"GPU util {u}%  mem {mu}/{mt} MiB  temp {t}C"
    except Exception:
        return "GPU: n/a"


# ---------------------------------------------------------------- HTML report
PALETTE = ["#2f6fdf", "#e0702a", "#2a9d6e", "#c43d5b", "#7b54c9", "#8a7a1a",
           "#1f9fb0", "#d0569f", "#5a6b7d", "#a65a2a", "#3c8c3c", "#9b2f2f"]


def svg_chart(series: list, title: str, ylabel: str, baseline=None, xkey="time_min") -> str:
    W, H, L, R, T, B = 640, 300, 56, 16, 28, 40
    pts = [(x, y) for s in series for x, y in s["pts"]]
    if not pts:
        return ""
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts] + ([baseline] if baseline is not None else [])
    x0, x1 = 0, max(xs) or 1
    y0, y1 = min(ys), max(ys)
    if y1 - y0 < 1e-9:
        y1 = y0 + 1
    pad = (y1 - y0) * 0.05
    y0, y1 = y0 - pad, y1 + pad
    sx = lambda x: L + (x - x0) / (x1 - x0) * (W - L - R)
    sy = lambda y: T + (1 - (y - y0) / (y1 - y0)) * (H - T - B)
    g = [f'<svg viewBox="0 0 {W} {H}" class="chart" role="img" aria-label="{html.escape(title)}">',
         f'<text x="{L}" y="18" class="ct">{html.escape(title)}</text>']
    for i in range(5):
        yv = y0 + (y1 - y0) * i / 4
        g.append(f'<line x1="{L}" x2="{W-R}" y1="{sy(yv):.1f}" y2="{sy(yv):.1f}" class="grid"/>')
        g.append(f'<text x="{L-6}" y="{sy(yv)+4:.1f}" class="tick" text-anchor="end">{yv:.2f}</text>')
    for i in range(5):
        xv = x0 + (x1 - x0) * i / 4
        g.append(f'<text x="{sx(xv):.1f}" y="{H-B+16}" class="tick" text-anchor="middle">{xv:.0f}</text>')
    g.append(f'<text x="{(L+W-R)/2}" y="{H-6}" class="tick" text-anchor="middle">{"minutes" if xkey=="time_min" else xkey}</text>')
    if baseline is not None:
        g.append(f'<line x1="{L}" x2="{W-R}" y1="{sy(baseline):.1f}" y2="{sy(baseline):.1f}" class="base"/>')
        g.append(f'<text x="{W-R-4}" y="{sy(baseline)-4:.1f}" class="tick" text-anchor="end">matching baseline</text>')
    for s in series:
        if not s["pts"]:
            continue
        d = " ".join(f"{'M' if i == 0 else 'L'}{sx(x):.1f},{sy(y):.1f}" for i, (x, y) in enumerate(s["pts"]))
        g.append(f'<path d="{d}" fill="none" stroke="{s["color"]}" stroke-width="2"><title>{html.escape(s["name"])}</title></path>')
    g.append("</svg>")
    return "\n".join(g)


def write_html(runs: list, path: Path) -> None:
    runs = [r for r in runs if r["iters"]]
    for i, r in enumerate(runs):
        r["color"] = PALETTE[i % len(PALETTE)]
    base = next((r for r in runs if r["baseline_val"]), None)
    mae = [{"name": r["name"], "color": r["color"],
            "pts": [(x["time"] / 60, x["val_mae"]) for x in r["iters"] if x.get("val_mae") is not None]} for r in runs]
    solve = [{"name": r["name"], "color": r["color"],
              "pts": [(x["time"] / 60, x["solve_rate"]) for x in r["iters"] if "solve_rate" in x]} for r in runs]
    loss = [{"name": r["name"], "color": r["color"],
             "pts": [(x["time"] / 60, x["loss"]) for x in r["iters"][2:] if x.get("loss") is not None]} for r in runs]
    legend = "".join(f'<span class="lg"><i style="background:{r["color"]}"></i>{html.escape(r["name"])}</span>' for r in runs)
    allr = load_all()
    rows = []
    for r in allr:
        for k, e in sorted(r["evals"].items()):
            rows.append((r["name"], k, e))
    rows.sort(key=lambda t: (t[1], -t[2].get("solve_rate", 0)))
    ev_html = "".join(
        f"<tr><td>{html.escape(n)}</td><td>{e.get('budget')}</td><td>{e.get('solved')}/{e.get('n')}</td>"
        f"<td>{100*e.get('solve_rate',0):.1f}%</td><td>{e.get('mean_expanded',0):.0f}</td>"
        f"<td>{e.get('mean_pushes',0):.1f}</td><td>{1000*e.get('mean_secs',0):.1f}</td></tr>"
        for n, k, e in rows)
    tbl = html.escape(table(allr))
    doc = f"""<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sokoban Heuristic Training</title>
<style>
:root {{ --bg:#fbfbfa; --fg:#1d1f23; --muted:#6b7078; --grid:#e3e4e6; --card:#ffffff; }}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{ --bg:#16181c; --fg:#e7e8ea; --muted:#9aa0a8; --grid:#2b2f36; --card:#1d2026; }} }}
:root[data-theme="dark"] {{ --bg:#16181c; --fg:#e7e8ea; --muted:#9aa0a8; --grid:#2b2f36; --card:#1d2026; }}
body {{ background:var(--bg); color:var(--fg); font:14px/1.5 system-ui,sans-serif; margin:0 auto; max-width:1100px; padding:16px; }}
h1 {{ font-size:20px; }} h2 {{ font-size:16px; margin-top:28px; }}
.muted {{ color:var(--muted); }}
.chart {{ width:100%; max-width:640px; background:var(--card); border-radius:8px; }}
.ct {{ fill:var(--fg); font-size:13px; font-weight:600; }}
.tick {{ fill:var(--muted); font-size:11px; }}
.grid {{ stroke:var(--grid); }} .base {{ stroke:var(--muted); stroke-dasharray:5 4; }}
.lg {{ display:inline-flex; align-items:center; gap:6px; margin:0 14px 6px 0; font-size:12px; }}
.lg i {{ width:14px; height:3px; display:inline-block; }}
pre {{ background:var(--card); padding:12px; border-radius:8px; overflow-x:auto; font-size:12px; }}
table {{ border-collapse:collapse; font-size:13px; }} td,th {{ padding:4px 10px; border-bottom:1px solid var(--grid); text-align:left; }}
.grid2 {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:16px; }}
.wrap {{ overflow-x:auto; }}
</style></head><body>
<h1>Sokoban neural heuristic training</h1>
<p class="muted">Updated {time.strftime('%Y-%m-%d %H:%M')} &middot; {html.escape(gpu_line())}</p>
<div>{legend}</div>
<div class="grid2">
{svg_chart(mae, "Validation MAE vs exact push distance (held-out levels)", "MAE", base["baseline_val"].get("val_mae") if base else None)}
{svg_chart(solve, "Solve rate, 128 held-out puzzles, 1000 expansions", "solve", base["baseline"].get("solve_rate") if base else None)}
{svg_chart(loss, "Training loss", "loss")}
</div>
<h2>Final evaluation on full held-out test split</h2>
<div class="wrap"><table><tr><th>run</th><th>budget</th><th>solved</th><th>rate</th><th>mean expanded</th><th>mean pushes</th><th>ms/puzzle</th></tr>{ev_html}</table></div>
<h2>Run table</h2><pre>{tbl}</pre>
</body></html>"""
    path.write_text(doc)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--watch", type=int, default=0, help="refresh interval seconds")
    ap.add_argument("--html", type=str, default="", help="write HTML report here")
    a = ap.parse_args()
    while True:
        runs = load_all()
        if a.watch:
            os.system("clear")
        print(time.strftime("%H:%M:%S"), gpu_line())
        print(table(runs))
        if a.html:
            write_html(runs, Path(a.html))
        if not a.watch:
            break
        time.sleep(a.watch)


if __name__ == "__main__":
    main()
