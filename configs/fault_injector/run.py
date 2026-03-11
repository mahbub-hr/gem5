import subprocess
import random
import os, sys
import logging
import multiprocessing
from collections import Counter
import argparse, json
from datetime import datetime

random.seed()  # For reproducibility


# CONFIGURATION
GEM5_BIN = "/work/host/gem5/build/X86/gem5.opt"
CONFIG_SCRIPT = "/work/host/gem5/configs/fault_injector/tests/random_mbu/fi_cfg.py"
BENCHMARK_CONFIG = "benchmark_configs.json"
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
logger = logging.getLogger(__name__)

def setup_logger(log_dir):
    """
    Sets up a dual-logging system for Academic Research.
    INFO level to console, DEBUG level to file.
    """
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(log_dir, "campaign_audit.log")

    root_logger = logging.getLogger()
    root_logger.setLevel(logging.DEBUG)
    
    # Remove existing handlers if any (to prevent double logging)
    root_logger.handlers = []

    # 1. File Handler (The "Lab Notebook" - Detailed)
    file_formatter = logging.Formatter('%(asctime)s - [%(levelname)s] - %(name)s: %(message)s')
    fh = logging.FileHandler(log_file)
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(file_formatter)
    root_logger.addHandler(fh)

    # 2. Console Handler (The "Dashboard" - Clean)
    console_formatter = logging.Formatter('[%(levelname)s] %(message)s')
    ch = logging.StreamHandler(sys.stdout)
    ch.setLevel(logging.INFO)
    ch.setFormatter(console_formatter)
    root_logger.addHandler(ch)

    return log_file

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
        logger.error(f"Error: Golden output file {golden_output_file} does not exist.")
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

def run_gem5(args, timeout=None):
    """Helper to run gem5 command"""
    cmd = [GEM5_BIN] + args
    # print(f"Executing: {' '.join(cmd)}")
    subprocess.run(
        cmd, 
        check=True
        ,stdout= subprocess.DEVNULL  # Suppress gem5's stdout
        ,stderr= subprocess.DEVNULL   # Suppress gem5's stderr
        ,timeout=timeout 
    )

    return


def classify_output(golden_output, program_output_file):
    if golden_output is None:
        logger.error("Panic: Golden output is not available for classification.")
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

        elif "error" in fi_output.lower():
            result_type = "DETECTED"

        elif fi_output != golden_output:
            result_type = "SDC"

        else:
            result_type = "MASKED"

    return result_type

# This is will the random inejction points generation logic inside the config script.
def single_injection_run_with_random_values(cmd, out_dir, program_output_filepath, max_tick, num_injections_per_run=1, num_bits_to_flip=1):
    global GOLDEN_OUTPUT, DELETE_PER_RUN_OUTPUT
    WALL_CLOCK_TIMEOUT = 300
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
        run_gem5(args, timeout=WALL_CLOCK_TIMEOUT)
    
    except subprocess.TimeoutExpired:
        logger.debug(f"\n[Warning] Run {out_dir} timed out (Infinite Loop/Hang). Killing...")
        result_type = "TIMEOUT"
        if DELETE_PER_RUN_OUTPUT and os.path.exists(out_dir):
            subprocess.run(["rm", "-rf", out_dir])
        return result_type
    
    except subprocess.CalledProcessError as e:
        logger.debug(f"\nError during injection run:\n {e}")
        result_type = "CRASH" #TODO: verify the crash
    
    result_type = classify_output(GOLDEN_OUTPUT, program_output_filepath)
    logger.info(f"\nCompleted Run: {out_dir}\n")

    if DELETE_PER_RUN_OUTPUT and os.path.exists(out_dir):
        subprocess.run(["rm", "-rf", out_dir])

    return result_type

def single_injection_run(config_name, injection_id, inject_ticks, target_sets, target_ways, target_byte_positions, byte_masks):
    num_of_injections = len(inject_ticks)

    logger.info(f"\n--- Injection Run {injection_id}/{num_of_injections} ---")

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
            logger.error(f"Error during golden run:\n {e}")
            logger.error(f"cmd:{GEM5_BIN} {' '.join(args)}")
            exit(1)

    max_tick = get_max_ticks(stats_file=os.path.join(out_dir, "stats.txt"))

    if max_tick == 0:
        logger.error("Error: Could not determine execution time.")
        exit(1)

    with open(os.path.join(out_dir, program_output_file), "r") as fr:
        GOLDEN_OUTPUT = fr.read()
    
    return max_tick


def parallel_injections(benchmark: str, config_name: str, num_of_injections: int):
    pool_size = max(1, multiprocessing.cpu_count() - 4)
    logger.info(f"\n=== Using multiprocessing pool of size: {pool_size}")

    cmd = " ".join(configs[benchmark][config_name]["cmd"])
    job_args = []
    
    logger.info(f"Preparing configuration for {num_of_injections} injections...")
    
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
    logger.info(f"Starting execution...")
    
    with multiprocessing.Pool(pool_size) as pool:
        # starmap calls: func(cmd, out_dir, prog_out, ...)
        results = pool.starmap(single_injection_run_with_random_values, job_args)

    # 5. Aggregate Results
    total_stats = Counter(results)
    
    return total_stats

def sequential_injections(benchmark: str, config_name: str, num_of_injections: int):
    cmd = " ".join(configs[benchmark][config_name]["cmd"])
    total_stats = Counter()
    logger.info(f"Running experiment {num_of_injections} times...")

    for injection_id in range(num_of_injections):
        logger.info(f"  > Starting Injection {injection_id+1}/{num_of_injections}...")
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

def run_benchmarks(benchmarks: list, per_benchmark_configs: list, num_of_injections_per_benchmark, run_parallel = False):
    global MAX_TICK, GOLDEN_OUTPUT
    ALL_STATISTICS = {}
    statistics_filepath = os.path.join(M5OUT_DIR, "fi_statistics.json")

    if benchmarks[0] == "all":
        benchmarks = configs.keys()
    
    if per_benchmark_configs[0] == "all":
        per_benchmark_configs = configs[benchmarks[0]].keys()

    print(f"{benchmarks} {per_benchmark_configs}")

    # clean_prev_data() # TODO
    for benchmark in benchmarks:
        GOLDEN_OUTPUT = None
        MAX_TICK = None
        ALL_STATISTICS[benchmark] = {}

        logger.info(f"\n\n==============================\n"
              f"=== Running Benchmark: {benchmark} ===\n"
              f"==============================\n")
        # out_dir = os.path.join(M5OUT_DIR, benchmark, "golden_run")
        # TODO: Every time we run, we are creating new directory.
        # Therefore, next time it will not find the previous data
        # which makes it to run the golden run again each time.
        MAX_TICK, GOLDEN_OUTPUT = get_stats_from_golden_run(benchmark)

        for cfg in per_benchmark_configs:
            if cfg == "golden_run":
                continue

            logger.info(f"--- Running Config: {cfg} ---")
            if run_parallel:
                injection_stats = parallel_injections(benchmark, cfg, num_of_injections_per_benchmark)
            
            else:
                injection_stats = sequential_injections(benchmark, cfg, num_of_injections_per_benchmark)
            
            ALL_STATISTICS[benchmark][cfg] = injection_stats
            save_data_for_plotting(ALL_STATISTICS, statistics_filepath)
    
    return ALL_STATISTICS

def print_statistics_to_console(statistics: dict):
    if statistics is None:
        logger.info("Unable to print statistics from this run. Something went wrong.")
        exit(-1)

    print(list(statistics.keys()), " ", list(statistics.values()))

    first_benchmark = list(statistics.keys())[0]
    first_config = list(list(statistics.values())[0].keys())[0]
    num_of_injections_per_benchmark_per_config = sum(list(statistics[first_benchmark][first_config].values()))

    for benchmark in statistics:
        single_benchmark = statistics[benchmark]
        for cfg in single_benchmark:
            if cfg == "golden_run":
                continue

            logger.info(
                f"\n=========================================\n"
                f"===      Config: {cfg}        ===\n"
                f"=========================================\n"
                )
            logger.info(f"**Total Injections per benchmark per config: {num_of_injections_per_benchmark_per_config}")

            single_config = single_benchmark[cfg]

            for key in single_config:
                avg_count = single_config[key]
                logger.info(f"{key:<10}   {avg_count:>10.2f} ")

    return

def init_log(benchmarks: list):
    global M5OUT_DIR
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    M5OUT_DIR = os.path.join(M5OUT_DIR, f"{timestamp}")
    os.makedirs(M5OUT_DIR, exist_ok=True)
    log_path = setup_logger(M5OUT_DIR)
    logger.info(f"LOG PATH: {log_path}")



def main():
    parser = argparse.ArgumentParser(description="Fault Injection Campaign Runner")
    fixed_point_injection_group = parser.add_argument_group("Fixed Injection Point Arguments")
    random_injection_group = parser.add_argument_group("Random Injection Point Arguments")
    profile_run_group = parser.add_argument_group("Profiling the program (Gold Run)")

    benchmark_choices = list(configs.keys()) + ["all"]
    parser.add_argument(
        "--benchmarks", 
        nargs="+", 
        choices=benchmark_choices, 
        default=["qsort_small"], 
        help="Which benchmarks to run or 'all' to run all the benchmarks"
    )
    config_choices = list(configs["matrix_mul"].keys()) + ["all"]
    parser.add_argument(
        "--configs", 
        nargs="+", 
        choices=config_choices, 
        default=["all"], 
        help="Which configs to run"
    )
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
        "--parallel", 
        action="store_true", 
        default=False,
        help="Fires up multiple processes to inject in parallel"
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
    BENCHMARKS = args.benchmarks
    ALL_STATISTICS = None

    init_log(BENCHMARKS)

    # --- PHASE 2: INJECTION CAMPAIGN ---
    if args.profile:
        logger.info(f"\n=== PHASE 1: PROFILING (Gold Run) ===")
        max_ticks = gold_run(configs[BENCHMARK]["golden_run"]["cmd"], out_dir=os.path.join(M5OUT_DIR, BENCHMARK, "golden_run"))
        logger.info(f"Max Ticks for Injection Point Generation: {max_ticks}")
        exit(0)

    # fixed injection point runs (for verification)
    if args.sets is not None:

        if args.ways is None:
            logger.error("Error: --ways is required for single injection runs")
            exit(1)
        if args.byte_positions is None:
            logger.error("Error: --byte-positions is required for single injection runs")
            exit(1)
        if args.byte_masks is None:
            logger.error("Error: --byte-masks is required for single injection runs")
            exit(1)

        ALL_STATISTICS[BENCHMARKS[0]][args.configs[0]] = single_injection_run(
            benchmark = args.benchmarks[0],
            config_name=args.configs[0],
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
            logger.error("Error: --num-of-injections is required for random injection campaigns")
            exit(1)

        num_of_injections = args.num_of_injections
        per_benchmark_configs = args.configs
        run_parallel = args.parallel
        ALL_STATISTICS = run_benchmarks(BENCHMARKS, per_benchmark_configs, num_of_injections, run_parallel)
        print_statistics_to_console(ALL_STATISTICS)
    
if __name__ == "__main__":
    main()