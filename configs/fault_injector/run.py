import subprocess
import random
import os
import re
import multiprocessing
from collections import Counter
# CONFIGURATION
GEM5_BIN = "/work/host/gem5/build/X86/gem5.opt"
CONFIG_SCRIPT = "/work/host/gem5/configs/fault_injector/tests/random_mbu/fi_cfg.py"
STATS_FILE = "m5out/stats.txt"
GOLDEN_OUTPUT_FILE = "/dev/shm/golden_output.out"
PROGRAM_OUTPUT_FILE = "m5out/program_output.txt"
GOLDEN_OUTPUT = None
NUM_INJECTIONS = 125

    

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
    subprocess.run(
        cmd, 
        check=True
    )

    return

def single_injection_run(injection_id, max_ticks):
    global crash, detected, masked, silent
    print(f"\n--- Injection Run {injection_id}/{NUM_INJECTIONS} ---")

    program_output_file = f"/dev/shm/program_output_{injection_id}.out"
    unique_m5_dir = f"m5out/run_{injection_id}"

    try:
        run_gem5([
            f"--outdir={unique_m5_dir}",
            # "--debug-flags=FI",
            # f"--debug-file=fi_trace.out",
            CONFIG_SCRIPT,
            f"--max-tick={max_ticks}",
            f"--output-file={program_output_file}"
        ])
        gem5_crashed = False
    except subprocess.CalledProcessError as e:
        gem5_crashed = True

    # --- Classification Logic ---
    result_type = "UNKNOWN"

    if gem5_crashed:
        result_type = "CRASH"
    
    elif not os.path.exists(program_output_file):
        result_type = "CRASH"
        
    else:
        with open(program_output_file, "r", errors="replace") as f:
            fi_output = f.read()

        if not fi_output:
            result_type = "CRASH"

        elif "error" in fi_output.lower():
            result_type = "DETECTED"

        elif fi_output != GOLDEN_OUTPUT:
            result_type = "SILENT"

        else:
            result_type = "MASKED"

    if os.path.exists(program_output_file):
        os.remove(program_output_file)

    return result_type

def main():
    global GOLDEN_OUTPUT
    print("\n=== PHASE 1: PROFILING (Gold Run) ===")

    run_gem5([
        CONFIG_SCRIPT, 
        "--profile", 
        f"--output-file={GOLDEN_OUTPUT_FILE}"
    ])
    max_ticks = get_max_ticks()
    print(f"Total Execution Time: {max_ticks} ticks")

    if max_ticks == 0:
        print("Error: Could not determine execution time.")
        exit(1)

    with open(GOLDEN_OUTPUT_FILE, "r") as f:
        GOLDEN_OUTPUT = f.read()

    # --- PHASE 2: INJECTION ---
    print("\n=== PHASE 2: INJECTION RUN ===")
    pool_size = multiprocessing.cpu_count() - 4
    print(f"\n=== Using multiprocessing pool of size: {pool_size}")
    job_args = [(i, max_ticks) for i in range(NUM_INJECTIONS)]
    with multiprocessing.Pool(pool_size) as pool:
        results = pool.starmap(single_injection_run, job_args)

    stats = Counter(results)
    
    print("\n=== FINAL STATISTICS ===")
    print(f"Total Injections: {NUM_INJECTIONS}")
    print(f"Masked:   {stats['MASKED']}")
    print(f"Silent:   {stats['SILENT']}")
    print(f"Detected: {stats['DETECTED']}")
    print(f"Crashes:  {stats['CRASH']}")

    print("\nDone. Check m5out/fi_trace.out for results.")

if __name__ == "__main__":
    main()