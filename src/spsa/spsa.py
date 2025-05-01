import subprocess
import random
import shutil
import re
import time
import os
import subprocess


# Coefficients to perturb and their ranges
param_ranges = {
    "historyLrmFactor": (1000, 20000),

    "rfpDepthCoeff": (0, 100),
    "rfpImprovingCoeff": (0, 2),
    "rfpDepthLimit": (0, 10),

    "singularDepthLimit": (0, 12),
    "tableDepthReductionLimit": (0, 4),
    "singularReductionFactor": (2, 5),

    "lmpDepthLimit": (1, 15),
    "lmpCoeff0": (1, 10),
    "lmpCoeff2": (1, 10),
    "lmpCoeff3": (1, 10),

    "histCoeff0": (500, 2000),
    "histCoeff1": (500, 3000),

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
# windows
# def build_engine(output):
#     try:
#         subprocess.check_call([
#             "/opt/homebrew/opt/llvm/bin/clang++", "-std=c++17", "-O3", "-march=native",
#             "-fopenmp", "-fopenmp-simd", "-pthread", "-Wall",
#             "-Wextra", "-Wshadow", "-w", "-static",
#             "-I", "include/", "-I", "../../lib/fathom/src",
#             "aku.cpp", "search_tune.cpp", "../../lib/fathom/src/tbprobe.c",
#             "-o", output, "-lm"
#         ])
#         return True
#     except subprocess.CalledProcessError:
#         return False

#MacOS
def build_engine(output):
    try:
        subprocess.check_call([
            "/opt/homebrew/opt/llvm/bin/clang++",
            "-std=c++17", "-O3", "-ffast-math", "-fopenmp",
            "-Wall", "-Wextra", "-Wshadow", "-w",
            "-I", "include/", "-I", "../../lib/fathom/src",
            "aku.cpp", "search_tune.cpp", "../../lib/fathom/src/tbprobe.c",
            "-o", output, "-lm"
        ])
        return True
    except subprocess.CalledProcessError as e:
        print("❌ Build failed.")
        print("Command:", e.cmd)
        return False

import subprocess
import os

import subprocess
import re

import subprocess
import os
import re

def evaluate_challenger():
    cmd = [
        "./fastchess",
        "-engine", "cmd=aku_test.exe", "name=Test",
        "-engine", "cmd=aku_best.exe", "name=Best",
        "-each", "tc=30+0.3",
        "-rounds", "2",
        "-repeat",
        "-concurrency", "4",
        "-pgnout", "file=tmp.pgn"
    ]

    try:
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        output_lines = []

        # Stream output live and capture it
        for line in process.stdout:
            print(line, end='')  # show in console immediately
            output_lines.append(line)

        process.wait()
        if process.returncode != 0:
            print("❌ fastchess failed to run.")
            return False

    except Exception as e:
        print(f"❌ Exception: {e}")
        return False

    # Combine captured output
    output = "".join(output_lines)

    # Parse summary
    match = re.search(r"Games:\s*(\d+),\s*Wins:\s*(\d+),\s*Losses:\s*(\d+),\s*Draws:\s*(\d+)", output)
    if not match:
        print("❌ Failed to parse summary.")
        return False

    games, wins, losses, draws = map(int, match.groups())
    print(f"Parsed → Games: {games}, Wins (Test): {wins}, Losses: {losses}, Draws: {draws}")

    return wins > losses



# Perturb a random parameter
def perturb(params):
    key = random.choice(list(param_ranges.keys()))
    current_val = params[key]
    low_bound, high_bound = param_ranges[key]

    # Compute ±20% range around current value
    delta = max(1, int(current_val * 0.1))
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
