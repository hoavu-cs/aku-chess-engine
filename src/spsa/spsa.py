import subprocess
import random
import shutil
import re
import time

# Coefficients to perturb and their ranges
param_ranges = {
    "historyLrmFactor": (1000, 20000),

    "rfpDepthCoeff": (0, 100),
    "rfpImprovingCoeff": (0, 200),
    "rfpDepthLimit": (0, 16),

    "singularDepthLimit": (0, 16),
    "tableDepthReductionLimit": (0, 8),
    "singularReductionFactor": (1, 4),

    "lmpDepthLimit": (1, 12),
    "lmpCoeff0": (1, 10),
    "lmpCoeff2": (1, 10),
    "lmpCoeff3": (1, 10),

    "histCoeff0": (100, 2000),
    "histCoeff1": (500, 5000),

    "seeCoeff1": (1, 500),
    "seeDepthLimit": (1, 32),

    "fpDepthLimit": (1, 12),
    "fpCoeff0": (1, 200),
    "fpCoeff1": (1, 200),
    "fpCoeff2": (1, 200),

    "maxHistory": (1000, 20000),
    "maxCaptureHistory": (1000, 10000),

    "deltaCoeff0": (0, 10),
    "deltaCoeff1": (0, 10),
    "deltaCoeff2": (1, 10),
}

# Load original parameters
def load_params():
    with open("parameters.hpp", "r") as f:
        lines = f.readlines()
    params = {}
    for line in lines:
        m = re.match(r"int (\w+) = (\d+);", line)
        if m:
            key, val = m.groups()
            params[key] = int(val)
    return params

# Write to parameters.hpp
def save_params(params):
    with open("parameters.hpp", "w") as f:
        f.write("#pragma once\n\n")
        for k, v in params.items():
            f.write(f"int {k} = {v};\n")

# Build engine with given output name
def build_engine(output):
    try:
        subprocess.check_call([
            "g++", "-std=c++17", "-O3", "-march=native",
            "-fopenmp", "-fopenmp-simd", "-pthread", "-Wall",
            "-Wextra", "-Wshadow", "-w", "-static",
            "-I", "include/", "-I", "../../lib/fathom/src",
            "aku.cpp", "search_tune.cpp", "../../lib/fathom/src/tbprobe.c",
            "-o", output, "-lm"
        ])
        return True
    except subprocess.CalledProcessError:
        return False

import os
import subprocess

def evaluate_challenger():
    cmd = [
        "fastchess.exe",
        "-engine", "cmd=aku_test.exe", "name=Test",
        "-engine", "cmd=aku_best.exe", "name=Best",
        "-each", "tc=40+0.4",
        "-rounds", "4",
        "-repeat",
        "-concurrency", "2",
        "-pgnout", "file=tmp.pgn"
    ]

    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    if not os.path.exists("tmp.pgn"):
        print("❌ fastchess.exe failed or did not produce PGN output.")
        print("----- STDOUT -----")
        print(result.stdout)
        print("----- STDERR -----")
        print(result.stderr)
        return False

    # Parse PGN for results
    wins_best = sum(1 for line in open("tmp.pgn") if 'Result "1-0"' in line)  # Best is White
    wins_test = sum(1 for line in open("tmp.pgn") if 'Result "0-1"' in line)  # Test is Black

    return wins_test > wins_best


# Perturb a random parameter
def perturb(params):
    key = random.choice(list(param_ranges.keys()))
    current_val = params[key]
    low_bound, high_bound = param_ranges[key]

    # Compute ±20% range around current value
    delta = max(1, int(current_val * 0.2))
    new_val = current_val + random.randint(-delta, delta)

    # Clamp to allowed range
    new_val = max(low_bound, min(high_bound, new_val))

    new_params = params.copy()
    new_params[key] = new_val
    return new_params, key, new_val


# Main loop
params = load_params()
save_params(params)
build_engine("aku_best.exe")

for i in range(1, 5000):
    trial_params, key, val = perturb(params)
    save_params(trial_params)
    print(f"[{i}] Trying perturbation: {key} = {val}")
    if not build_engine("aku_test.exe"):
        print("Build failed, skipping...")
        continue
    if evaluate_challenger():
        print("Challenger won. Updating best.")
        shutil.copyfile("aku_test.exe", "aku_best.exe")
        params = trial_params
        save_params(params)  # <-- ✅ Save best back to parameters.hpp
    else:
        print("Challenger lost.")
