# @author: mahbub
# @file: fi_cfg.py

import m5
from m5.objects import *
import random
import argparse
import os
import time

# TODO: not strictly sporadic.
def sporadic_byte_mask(num_bits_to_flip):
    """Generates a byte mask with a specific number of bits set to 1"""
    if num_bits_to_flip > 8:
        raise ValueError("num_bits_to_flip cannot exceed 8 for a single byte.")
    
    bit_positions = random.sample(range(8), num_bits_to_flip)
    mask = 0
    for pos in bit_positions:
        mask |= (1 << pos)
    
    return mask

# In a byte (8 bits), there are 7 possible pairs of consecutive bits:
# (0,1), (1,2), (2,3), (3,4), (4,5), (5,6), and (6,7).
# TODO: NOT USED
def consecuctive_byte_mask(num_bits_to_flip):
    """Generates a byte mask with a specific number of consecutive bits set to 1"""
    if num_bits_to_flip > 8:
        raise ValueError("num_bits_to_flip cannot exceed 8 for a single byte.")
    
    # 2. Create a mask of 'n' consecutive ones
    # A trick for 'n' ones is (2^n - 1)
    # Then shift it to the start position
    start_pos = random.randint(0, 8 - num_bits_to_flip)
    mask = ((1 << num_bits_to_flip) - 1) << start_pos
    return mask

def generate_random_injection_points(max_tick, num_injections_per_run=1, num_bits_to_flip=1, NUM_SETS=64, L1DCACHE_ASSOC=2, NUM_BYTES_PER_WAY=64):
    START_TICK = int(max_tick * 0.10) 
    END_TICK   = int(max_tick * 0.95)
    random_ticks = sorted(random.sample(range(START_TICK, END_TICK), num_injections_per_run))
    target_sets = []
    target_ways = []
    target_byte_positions = []
    target_byte_masks = []

    for i in range(num_injections_per_run):
        s = random.randint(0, NUM_SETS - 1)
        w = random.randint(0, L1DCACHE_ASSOC - 1)
        b_pos = random.randint(0, NUM_BYTES_PER_WAY - 1)
        num_bits_to_flip = 8#random.randint(1, 8)
        mask  = sporadic_byte_mask(num_bits_to_flip)

        # D. Append to lists
        target_sets.append(s)
        target_ways.append(w)
        target_byte_positions.append(b_pos)
        target_byte_masks.append(mask)

        # Print verification for this specific injection
        print(f"\nInj #{i+1} @ Tick {random_ticks[i]}: "
            f"Set={s:2d}, Way={w:1d}, Byte={b_pos:2d}, Mask={mask:02x} "
            f"Number of bits to flip: {num_bits_to_flip}\n"
            )
    return random_ticks, target_sets, target_ways, target_byte_positions, target_byte_masks


# -------------------------------------------------------------------------
# 1. Argument Parsing
# Please only specify arguments that are relevant to per run.
# -------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument("--profile", action="store_true", help="Run without injection to get max ticks")
parser.add_argument("--max-tick", type=int, default=0, help="The maximum tick count for random injection point generation.")
parser.add_argument("--inject-ticks", type=int, default=0, help="The ticks at which to inject faults.")
parser.add_argument("--output-file", type=str, default="program_output.txt", help="File to store program output")
parser.add_argument("--l1dassoc", type=int, default=2, help="Associativity of the L1 D-Cache")
parser.add_argument("--l1iassoc", type=int, default=2, help="Associativity of the L1 I-Cache")
parser.add_argument("--l2assoc", type=int, default=8, help="Associativity of the L2 Cache")
parser.add_argument("--cmd", type=str, required=True, help="Command to run the program.")
parser.add_argument("--sets", nargs="+", type=int, default=None, help="Cache set to target for injection")
parser.add_argument("--ways", nargs="+", type=int, default=None, help="Cache way to target for injection")
parser.add_argument("--byte-positions", nargs="+", type=int, default=None, help="Byte positions in a block to target for injection")
parser.add_argument("--byte-masks", nargs="+", type=int, default=None, help="Byte mask to flip bits at target positions")
parser.add_argument("--length", nargs="+", type=int, default=None, help="Length of the injection in bits")
parser.add_argument("--num-of-bits-to-flip", type=int, default=1, help="Number of bits to flip in the target byte")
parser.add_argument("--dump-cache-content", action="store_true", default=False, help="Dump cache content before and after injection")
parser.add_argument("--generate-random-injection-points", action="store_true", default=True, help="Generate random injection points instead of using specified ones")
parser.add_argument("--num-of-injections-per-run", type=int, default=1, help="Number of random injections to perform")
parser.add_argument("--seed", type=int, default=None, help="Random seed for reproducibility")

# ... other args ...
options, unknown = parser.parse_known_args()
cmd = options.cmd
inject_ticks = options.inject_ticks
target_component = "dcache"  # For this config, we focus on D-Cache injections
L1DCACHE_ASSOC = options.l1dassoc  # Default to 2-way set associative if not provided
target_sets = options.sets
target_ways = options.ways
target_bit_positions = options.byte_positions
target_byte_masks = options.byte_masks
dump_cache_content = options.dump_cache_content
GENERATE_RANDOM_INJECTION_POINTS = options.generate_random_injection_points
SEED = None

if options.seed is not None:
    SEED = options.seed
else:
    SEED = time.time_ns()
    
random.seed(SEED)
# -------------------------------------------------------------------------
# 2. Configuration Constants & Setup
# -------------------------------------------------------------------------
MAX_TICK = options.max_tick # only needed for random injection point generation
L1DCACHE_SIZE = (4*L1DCACHE_ASSOC)*1024  # 32KB 
L1DCACHE_BLOCK_SIZE = 64  # 64B
NUM_SETS = L1DCACHE_SIZE // (L1DCACHE_ASSOC * L1DCACHE_BLOCK_SIZE)  # 64 sets
BITS_PER_BYTE = 8
BITS_PER_BLOCK = L1DCACHE_BLOCK_SIZE * BITS_PER_BYTE  # 512 bits per block
PROGRAM_OUTPUT = options.output_file if options.output_file else "program_output.txt"
NUM_OF_BITS_TO_FLIP = options.num_of_bits_to_flip
# -------------------------------------------------------------------------
# 3. System Construction (CPU)
# -------------------------------------------------------------------------
system = System()
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
system.mem_ranges = [AddrRange("4GiB")]
system.cpu = TimingSimpleCPU()
system.cpu.isa = [X86ISA() for i in range(system.cpu.numThreads)]

system.cpu.dcache = Cache(
    size= f"{L1DCACHE_SIZE}B",
    assoc=L1DCACHE_ASSOC,
    tag_latency=2,
    data_latency=2,
    response_latency=2,
    mshrs=4,
    tgts_per_mshr=20,
)

# -------------------------------------------------------------------------
# 4. System Connections & Port Wiring (Hardware Wiring)
# -------------------------------------------------------------------------
system.membus = SystemXBar()

# B.Todo: Add a separate L1ICache
system.cpu.icache_port = system.membus.cpu_side_ports

# C. Connect CPU Data Port to Cache, and Cache to Bus
system.cpu.dcache.cpu_side = system.cpu.dcache_port
system.cpu.dcache.mem_side = system.membus.cpu_side_ports

# D. Connect Interrupts (x86 requirement)
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# E. Memory Controller
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports
system.system_port = system.membus.cpu_side_ports

# -------------------------------------------------------------------------
# 5. Process Setup
# -------------------------------------------------------------------------
system.workload = SEWorkload.init_compatible(cmd)
process = Process()
process.cmd = cmd
process.output = os.path.join(PROGRAM_OUTPUT)
process.errout = os.path.join(PROGRAM_OUTPUT)
system.cpu.workload = process
system.cpu.createThreads()

# -------------------------------------------------------------------------
# 6. Fault Injector Logic (MOVED HERE)
# -------------------------------------------------------------------------
#TODO: sporadic bit flips at different byte positions across multiple blocks (not just a single set/way)
if not options.profile:

    if GENERATE_RANDOM_INJECTION_POINTS:
        if options.num_of_injections_per_run is None:
            print(f"Continuing with default number of injections: {options.num_of_injections_per_run}")

        inject_ticks, target_sets, target_ways, target_byte_positions, target_byte_masks = \
        generate_random_injection_points(
            max_tick=MAX_TICK,  # Placeholder, will be set after profiling run
            num_injections_per_run=options.num_of_injections_per_run,
            num_bits_to_flip=NUM_OF_BITS_TO_FLIP,
            NUM_SETS=NUM_SETS,
            L1DCACHE_ASSOC=L1DCACHE_ASSOC,
            NUM_BYTES_PER_WAY=L1DCACHE_BLOCK_SIZE
        )

        # write

    # 1. Define Injection Parameters
    system.injector = FaultInjector(
        inject_ticks=inject_ticks,  
        target_sets=target_sets,
        target_ways=target_ways,
        target_byte_positions=target_byte_positions,
        target_byte_masks=target_byte_masks,
        # random_values=random_values,
        dump_cache_content=dump_cache_content,
        target_object=system.cpu.dcache
    )

# -------------------------------------------------------------------------
# 7. Instantiate and Run
# -------------------------------------------------------------------------

root = Root(full_system=False, system=system)
m5.instantiate()
print("Beginning simulation!")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
