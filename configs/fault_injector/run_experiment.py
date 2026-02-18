import subprocess
import random
import os
import re

# CONFIGURATION
GEM5_BIN = "/work/host/gem5/build/X86/gem5.opt"
CONFIG_SCRIPT = "/work/host/gem5/configs/fault_injector/tests/random_mbu/fi_cfg.py"
STATS_FILE = "m5out/stats.txt"
NUM_INJECTIONS = 5

def get_max_ticks():
    """Reads stats.txt to find simTicks"""
    if not os.path.exists(STATS_FILE):
        return 0
    
    with open(STATS_FILE, "r") as f:
        for line in f:
            if "simTicks" in line:
                # Line format: simTicks    12345678   # Description
                parts = line.split()
                return int(parts[1])
    return 0

def run_gem5(args):
    """Helper to run gem5 command"""
    cmd = [GEM5_BIN] + args
    print(f"Executing: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)

def main():
    print("\n=== PHASE 1: PROFILING (Gold Run) ===")
    run_gem5([
        CONFIG_SCRIPT, 
        "--profile"
    ])
    max_ticks = get_max_ticks()
    print(f"Total Execution Time: {max_ticks} ticks")

    if max_ticks == 0:
        print("Error: Could not determine execution time.")
        exit(1)

    # --- PHASE 2: INJECTION ---
    print("\n=== PHASE 2: INJECTION RUN ===")

    run_gem5([
        "--debug-flags=FI",
        f"--debug-file=fi_trace.out",
        CONFIG_SCRIPT,
        f"--max-tick={max_ticks}"
    ])

    print("\nDone. Check m5out/fi_trace.out for results.")

if __name__ == "__main__":
    main()