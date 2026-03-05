import subprocess
import random
import os
import re
import multiprocessing
from collections import Counter
import argparse, json
from datetime import datetime

random.seed()  # For reproducibility


# CONFIGURATION
GEM5_BIN = "/work/host/gem5/build/X86/gem5.opt"
CONFIG_SCRIPT = "/work/host/gem5/configs/fault_injector/tests/random_mbu/fi_cfg.py"
BENCHMARK_CONFIG = "configs.json"
M5OUT_DIR = "m5out"
STATS_FILE = "m5out/golden_run/stats.txt"
GOLDEN_OUTPUT = None
MAX_TICK = None
PROGRAM_OUTPUT_FILE = "program_output.out"
DELETE_PER_RUN_OUTPUT = True

NUM_TRIALS = 1
BIT_FLIP_COUNT = [1, 2, 4, 8, 16, 32, 64]

L1DCACHE_ASSOC = 2  
L1DCACHE_SIZE = (4*L1DCACHE_ASSOC)*1024  # 32KB 
L1DCACHE_BLOCK_SIZE = 64  # 64B
NUM_SETS = L1DCACHE_SIZE // (L1DCACHE_ASSOC * L1DCACHE_BLOCK_SIZE)  # 64 sets
BITS_PER_BYTE = 8

configs = json.load(open(BENCHMARK_CONFIG, "r"))

def get_max_ticks(stats_file=STATS_FILE):
    """Reads stats.txt to find simTicks"""
    if not os.path.exists(stats_file):
        return 0
    
    with open(stats_file, "r") as f:
        for line in f:
            if "simTicks" in line:
                # Line format: simTicks    12345678   # Description
                parts = line.split()
                return int(parts[1])
    return 0

def read_golden_output(golden_output_file):
    if not os.path.exists(golden_output_file):
        print(f"Error: Golden output file {golden_output_file} does not exist.")
        return None

    with open(golden_output_file, "r") as f:
        return f.read()
    
    # TODO: verify the golden output is valid (not empty, not an error message, etc.)

def get_max_ticks_guranteed(benchmark: str, out_dir: str):
    max_tick = get_max_ticks(stats_file=os.path.join(out_dir, "stats.txt"))
    
    if max_tick == 0:
        gold_run(configs[benchmark]["golden_run"]["cmd"], out_dir=out_dir)
        max_tick = get_max_ticks(stats_file=os.path.join(out_dir, "stats.txt"))
    
    return max_tick

def  get_stats_from_golden_run(benchmark: str):
    out_dir = os.path.join(M5OUT_DIR, benchmark, "golden_run")
    program_output_filepath = os.path.join(out_dir, PROGRAM_OUTPUT_FILE)
    max_tick = get_max_ticks_guranteed(benchmark, out_dir)
    golden_output = read_golden_output(program_output_filepath)

    return max_tick, golden_output

def run_gem5(args):
    """Helper to run gem5 command"""
    cmd = [GEM5_BIN] + args
    print(f"Executing: {' '.join(cmd)}")
    subprocess.run(
        cmd, 
        check=True
        ,stdout= subprocess.DEVNULL,  # Suppress gem5's stdout
        stderr= subprocess.DEVNULL   # Suppress gem5's stderr
    )

    return


def classify_output(golden_output, program_output_file):
    if golden_output is None:
        print("Panic: Golden output is not available for classification.")
        exit(1)

    # --- Classification Logic ---
    result_type = "UNKNOWN"
    
    if not os.path.exists(program_output_file):
        result_type = "CRASH"
        
    else:
        with open(program_output_file, "r", errors="replace") as f:
            fi_output = f.read()

        if not fi_output:
            result_type = "CRASH"
            print("\n--- Output Mismatch Detected ---")
            print("Golden Output:")
            print(golden_output)
            print("\nFaulty Output:")
            print(fi_output)
            print("\n-------------------------------\n")

        elif "error" in fi_output.lower():
            result_type = "DETECTED"

        elif fi_output != golden_output:
            result_type = "SDC"

        else:
            result_type = "MASKED"

    if os.path.exists(program_output_file):
        os.remove(program_output_file)

    return result_type

# This is will the random inejction points generation logic inside the config script.
def single_injection_run_with_random_values(cmd, out_dir, program_output_filepath, max_tick, num_injections_per_run=1, num_bits_to_flip=1):
    global GOLDEN_OUTPUT, DELETE_PER_RUN_OUTPUT

    args = [
            f"--outdir={out_dir}",
            CONFIG_SCRIPT,
            "--generate-random-injection-points",
            f"--max-tick={max_tick}",
            f"--num-of-injections-per-run={num_injections_per_run}",
            f"--num-of-bits-to-flip={num_bits_to_flip}",
            f"--cmd={cmd}",
            f"--output-file={PROGRAM_OUTPUT_FILE}"
        ]
    try:
        run_gem5(args)
    except subprocess.CalledProcessError as e:
        print(f"\nError during injection run:\n {e}")
        result_type = "CRASH" #TODO: verify the crash
    
    result_type = classify_output(GOLDEN_OUTPUT, program_output_filepath)

    if DELETE_PER_RUN_OUTPUT and os.path.exists(out_dir):
        subprocess.run(["rm", "-rf", out_dir])

    return result_type

def single_injection_run(config_name, injection_id, inject_ticks, target_sets, target_ways, target_byte_positions, byte_masks):
    num_of_injections = len(inject_ticks)

    print(f"\n--- Injection Run {injection_id}/{num_of_injections} ---")

    program_output_file = f"/dev/shm/program_output_{injection_id}.out"
    unique_m5_dir = f"m5out/{config_name}/run_{injection_id}"
    command = " ".join(cmd)

    try:
        run_gem5([
            f"--outdir={unique_m5_dir}",
            # "--debug-flags=FI",
            # f"--debug-file=fi_trace.out",
            CONFIG_SCRIPT,
            f"--cmd={command}",
            f"--inject-ticks={inject_ticks}",
            f"--target-sets={target_sets}",
            f"--target-ways={target_ways}",
            f"--target-byte-positions={target_byte_positions}",
            f"--target-byte-masks={byte_masks}",
            f"--output-file={program_output_file}"
        ])
        gem5_crashed = False
    except subprocess.CalledProcessError as e:
        return "CRASH"

    if DELETE_PER_RUN_OUTPUT and os.path.exists(unique_m5_dir):
        subprocess.run(["rm", "-rf", unique_m5_dir])
        
    return classify_output(program_output_file)


def gold_run(cmd, out_dir):
    global GOLDEN_OUTPUT
    program_output_file =  "program_output.out"

    if not os.path.exists(os.path.join(out_dir, "stats.txt")):
        command = " ".join(cmd)
        args = [
            f"--outdir={out_dir}",
            CONFIG_SCRIPT, 
            "--profile",
            f"--cmd={command}", 
            f"--output-file={program_output_file}"
        ]
        try:
            run_gem5(args)
        except subprocess.CalledProcessError as e:
            subprocess.run(["rm", "-rf", out_dir])
            print(f"Error during golden run:\n {e}")
            print(f"cmd:{GEM5_BIN} {' '.join(args)}")
            exit(1)

    max_tick = get_max_ticks(stats_file=os.path.join(out_dir, "stats.txt"))

    if max_tick == 0:
        print("Error: Could not determine execution time.")
        exit(1)

    with open(os.path.join(out_dir, program_output_file), "r") as fr:
        GOLDEN_OUTPUT = fr.read()
    
    return max_tick

def parallel_injections(benchmark: str, config_name: str, num_of_injections: int):
    pool_size = max(1, multiprocessing.cpu_count() - 4)
    print(f"\n=== Using multiprocessing pool of size: {pool_size}")

    cmd = " ".join(configs[benchmark][config_name]["cmd"])
    job_args = []
    
    print(f"Preparing configuration for {num_of_injections} injections...")
    
    for injection_id in range(num_of_injections):
        out_dir = os.path.join(M5OUT_DIR, benchmark, config_name, f"run_{injection_id}")
        program_output_filepath = os.path.join(out_dir, PROGRAM_OUTPUT_FILE)
        
        job_args.append(( 
            cmd,
            out_dir,
            program_output_filepath,
            MAX_TICK,
            1, # num_injections_per_run
            1  # num_bits_to_flip
        ))

    # 4. Execute Parallel Runs
    print(f"Starting execution...")
    
    with multiprocessing.Pool(pool_size) as pool:
        # starmap calls: func(cmd, out_dir, prog_out, ...)
        results = pool.starmap(single_injection_run_with_random_values, job_args)

    # 5. Aggregate Results
    total_stats = Counter(results)
    
    return total_stats

def sequential_injections(benchmark: str, config_name: str, num_of_injections: int):
    cmd = " ".join(configs[benchmark][config_name]["cmd"])
    total_stats = Counter()
    print(f"Running experiment {num_of_injections} times...")

    for injection_id in range(num_of_injections):
        print(f"  > Starting Injection {injection_id+1}/{num_of_injections}...")
        out_dir = os.path.join(M5OUT_DIR, benchmark, config_name, f"run_{injection_id}")
        program_output_filepath = os.path.join(out_dir, PROGRAM_OUTPUT_FILE)

        result = single_injection_run_with_random_values(
            cmd,
            out_dir,
            program_output_filepath,
            MAX_TICK,
            num_injections_per_run=1,
            num_bits_to_flip=1 # For now, just flip 1 bit
        )

        total_stats[result] += 1

    return total_stats

def save_data_for_plotting(statistics:dict, filepath: str):
    with open(filepath, "w") as f:
        json.dump(statistics, f, indent=4)

def run_all_benchmarks(num_of_injections_per_benchmark):
    global MAX_TICK, GOLDEN_OUTPUT, M5OUT_DIR
    ALL_STATISTICS = {}
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    M5OUT_DIR = os.path.join(M5OUT_DIR, f"{timestamp}")
    statistics_filepath = os.path.join(M5OUT_DIR, "fi_statistics.json")
    os.makedirs(M5OUT_DIR, exist_ok=True)

    # clean_prev_data() # TODO
    for benchmark in configs:
        GOLDEN_OUTPUT = None
        MAX_TICK = None
        ALL_STATISTICS[benchmark] = {}

        print(f"\n\n==============================\n"
              f"=== Running Benchmark: {benchmark} ===\n"
              f"==============================\n")
        out_dir = os.path.join(M5OUT_DIR, benchmark, "golden_run")
        MAX_TICK, GOLDEN_OUTPUT = get_stats_from_golden_run(benchmark)

        for cfg in configs[benchmark]:
            if cfg == "golden_run":
                continue

            print(f"\n--- Running Config: {cfg} ---")
            paralle_injection_stats = parallel_injections(benchmark, cfg, num_of_injections_per_benchmark)
            ALL_STATISTICS[benchmark][cfg] = paralle_injection_stats

    save_data_for_plotting(ALL_STATISTICS, statistics_filepath)


def main():
    parser = argparse.ArgumentParser(description="Fault Injection Campaign Runner")
    fixed_point_injection_group = parser.add_argument_group("Fixed Injection Point Arguments")
    random_injection_group = parser.add_argument_group("Random Injection Point Arguments")
    profile_run_group = parser.add_argument_group("Profiling the program (Gold Run)")

    benchmark_choices = list(configs.keys()) + ["all"]
    parser.add_argument(
        "--benchmark", 
        type=str, 
        choices=benchmark_choices, 
        default="qsort_small", 
        help="Which benchmark to run or 'all' to run all the benchmarks")
    profile_run_group.add_argument(
        "--profile", 
        action="store_true", 
        help="Run without injection to get max ticks"
    )
    parser.add_argument(
        "--keep-per-run-output", 
        action="store_true", 
        default=False, 
        help="Delete program output files after classification"
    )

    # Random injection campaign args
    config_choices = list(configs["matrix_mul"].keys()) + ["all"]
    random_injection_group.add_argument(
        "--config", 
        type=str, 
        choices=config_choices, 
        default="all", 
        help="Which config to run"
    )
    random_injection_group.add_argument(
        "--seed", 
        type=int, 
        default=42, 
        help="Random seed for reproducibility"
    )
    random_injection_group.add_argument(
        "--num-of-injections", 
        type=int, 
        default=None, 
        help="Number of injections to perform"
    )
    random_injection_group.add_argument(
        "--parallel-injections", 
        action="store_true", 
        default=False,
        help="Run injections in parallel"
    )

    # fixed injection point args (for single injection runs)
    fixed_point_injection_group.add_argument(
        "--sets", 
        nargs="+", 
        type=int, 
        default=None, 
        help="Cache set to target for injection"
    )
    fixed_point_injection_group.add_argument(
        "--ways", 
        nargs="+", 
        type=int, 
        default=None, 
        help="Cache way to target for injection"
    )
    fixed_point_injection_group.add_argument(
        "--byte-positions", 
        nargs="+", 
        type=int, 
        default=None, 
        help="Byte positions in a block to target for injection"
    )
    fixed_point_injection_group.add_argument(
        "--byte-masks", 
        nargs="+", 
        type=int, 
        default=None, 
        help="Byte mask to flip bits at target positions"
    )

    
    global GOLDEN_OUTPUT, MAX_TICK, DELETE_PER_RUN_OUTPUT
    args = parser.parse_args()
    DELETE_PER_RUN_OUTPUT = not args.keep_per_run_output
    total_stats = Counter()
    BENCHMARK = args.benchmark
    total_stats = {}

    if args.benchmark == "all":
        if not args.num_of_injections:
            print("Error: --num-of-injections is required when running all benchmarks")
            exit(1)
        run_all_benchmarks(args.num_of_injections)
        return

    # --- PHASE 2: INJECTION CAMPAIGN ---
    if args.profile:
        print(f"\n=== PHASE 1: PROFILING (Gold Run) ===")
        max_ticks = gold_run(configs[BENCHMARK]["golden_run"]["cmd"], out_dir=os.path.join(M5OUT_DIR, BENCHMARK, "golden_run"))
        print(f"Max Ticks for Injection Point Generation: {max_ticks}")
        exit(0)

    # fixed injection point runs (for verification)
    if args.sets is not None:

        if args.ways is None:
            print("Error: --ways is required for single injection runs")
            exit(1)
        if args.byte_positions is None:
            print("Error: --byte-positions is required for single injection runs")
            exit(1)
        if args.byte_masks is None:
            print("Error: --byte-masks is required for single injection runs")
            exit(1)

        total_stats = single_injection_run(
            config_name=args.config,
            injection_id=1,
            inject_ticks=args.inject_ticks,
            target_sets=args.sets,
            target_ways=args.ways,
            target_byte_positions=args.byte_positions,
            byte_masks=args.byte_masks
        )

    # random injection point runs
    else:
        if args.num_of_injections is None:
            print("Error: --num-of-injections is required for random injection campaigns")
            exit(1)

        num_of_injections = args.num_of_injections
        MAX_TICK, GOLDEN_OUTPUT = get_stats_from_golden_run(BENCHMARK)

        if MAX_TICK is None or MAX_TICK == 0:
            print("Error: Could not determine max ticks for injection point generation.")
            exit(1)

        if args.parallel_injections:
            print("\n=== PHASE 2: INJECTION CAMPAIGN (Parallel) ===")
            if args.config == "duplicated" or args.config == "partitioned_duplicated":
                total_stats = parallel_injections(BENCHMARK, args.config, num_of_injections)
        
            else:
                for cfg in single_benchmrak_configs:
                    if cfg == "golden_run":
                        continue

                    print(f"\n--- Running Config: {cfg} ---")
                    total_stats[cfg] = parallel_injections(BENCHMARK, cfg, num_of_injections)

        else:
            print("\n=== PHASE 2: INJECTION CAMPAIGN (Sequential) ===")
           
            if args.config == "duplicated" or args.config == "partitioned_duplicated":
                total_stats[args.config] = sequential_injections(BENCHMARK, args.config, num_of_injections)

            else:
                for cfg in single_benchmrak_configs:
                    if cfg == "golden_run":
                        continue

                    print(f"\n--- Running Config: {cfg} ---")
                    total_stats[cfg] = sequential_injections(BENCHMARK, cfg, num_of_injections)

    # --- PHASE 3: RESULTS SUMMARY ---
    for config_name in total_stats:
        if config_name == "golden_run":
            continue

        print(f"\n=========================================\n"
              f"===      Config: {config_name}        ===\n"
              f"=========================================\n"
            )
        print(f"**Total Injections per run: {args.num_of_injections}")

        keys_of_interest = ['MASKED', 'SDC', 'DETECTED', 'CRASH']

        for key in keys_of_interest:
            avg_count = total_stats[config_name][key] / NUM_TRIALS
            print(f"{key:<10},   {avg_count:>10.2f} ")

if __name__ == "__main__":
    main()