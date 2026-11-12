"""Experiment definitions for scripts/sweep.py: STAGES maps a stage name to a
list of (run_name, puzzle_train args)."""

# shared training schedule for architecture screening (equal data budget)
SCREEN = ["--iters", 120, "--samples", 131072, "--batch", 512, "--lr", 1e-3,
          "--kmax-start", 20, "--kmax-end", 100, "--kmax-ramp", 0.3,
          "--bellman-from", 3, "--eval-every", 10, "--eval-levels", 128,
          "--eval-budget", 1000]

STAGES = {
    # stage 1: architecture family / head / global-context screening
    "screen": [
        ("s1_cnn_c64_b4_gap",       ["--arch", "cnn", "--channels", 64, "--blocks", 4, "--head", "gap"] + SCREEN),
        ("s1_cnn_c64_b4_flat",      ["--arch", "cnn", "--channels", 64, "--blocks", 4, "--head", "flat"] + SCREEN),
        ("s1_res_c64_b6_gap",       ["--arch", "resnet", "--channels", 64, "--blocks", 6, "--head", "gap"] + SCREEN),
        ("s1_res_c64_b6_flat",      ["--arch", "resnet", "--channels", 64, "--blocks", 6, "--head", "flat"] + SCREEN),
        ("s1_res_c64_b6_flat_se",   ["--arch", "resnet", "--channels", 64, "--blocks", 6, "--head", "flat", "--se"] + SCREEN),
        ("s1_res_c64_b6_flat_dil",  ["--arch", "resnet", "--channels", 64, "--blocks", 6, "--head", "flat", "--dilate"] + SCREEN),
        ("s1_res_c128_b10_flat",    ["--arch", "resnet", "--channels", 128, "--blocks", 10, "--head", "flat"] + SCREEN),
        ("s1_attn_d128_l4",         ["--arch", "attn", "--channels", 128, "--blocks", 4, "--heads", 4, "--lr", 5e-4] + SCREEN[:6] + SCREEN[8:]),
    ],
}
