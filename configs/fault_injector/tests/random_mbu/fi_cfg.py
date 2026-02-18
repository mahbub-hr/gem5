import m5
from m5.objects import *
import random
import argparse

# -------------------------------------------------------------------------
# 1. Argument Parsing
# -------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument("--profile", action="store_true", help="Run without injection to get max ticks")
parser.add_argument("--max-tick", type=int, default=0, help="The max tick of the binary from profile run.")
# ... other args ...
options = parser.parse_args()

# -------------------------------------------------------------------------
# 2. Configuration Constants & Setup
# -------------------------------------------------------------------------
L1DCACHE_SIZE = 8192  # 8KB
L1DCACHE_ASSOC = 2  
L1DCACHE_BLOCK_SIZE = 64  # 64B
NUM_SETS = L1DCACHE_SIZE // (L1DCACHE_ASSOC * L1DCACHE_BLOCK_SIZE)  # 64 sets
BITS_PER_BYTE = 8
BITS_PER_BLOCK = L1DCACHE_BLOCK_SIZE * BITS_PER_BYTE  # 512 bits per block

config = {
    "num_injections": 15,
    "target_component": "dcache",
    "seed" : 42,
    "dump_cache_content": False,
    # run from /work/host/gem5/configs/fault_injector
    "cmd": "/work/host/gem5/configs/fault_injector/tests/random_mbu/verify_fi.bin",
}

random.seed()  # For reproducibility

# -------------------------------------------------------------------------
# 3. System Construction (CPU)
# -------------------------------------------------------------------------
system = System()
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MiB")]
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

system.workload = SEWorkload.init_compatible(config["cmd"])
process = Process()
process.cmd = [config["cmd"]]  # Dummy command
system.cpu.workload = process
system.cpu.createThreads()

# -------------------------------------------------------------------------
# 6. Fault Injector Logic (MOVED HERE)
# -------------------------------------------------------------------------

if not options.profile:
    # 1. Define Injection Parameters
    NUM_INJECTIONS = config["num_injections"]
    START_TICK = int(options.max_tick * 0.10) 
    END_TICK   = int(options.max_tick * 0.95)

    random_ticks = sorted(random.sample(range(START_TICK, END_TICK), NUM_INJECTIONS))
    target_sets = []
    target_ways = []
    target_byte_positions = []
    target_lengths = []
    random_values = []

    print(f"--- Generating {NUM_INJECTIONS} Fault Scenarios ---")

    for i in range(NUM_INJECTIONS):
        # A. Random Set and Way
        s = random.randint(0, NUM_SETS - 1)
        w = random.randint(0, L1DCACHE_ASSOC - 1)
        
        # B. Random Byte Offset within the block (0 to 63)
        b_pos = random.randint(0, L1DCACHE_BLOCK_SIZE - 1)
        
        # C. Random Length (Must fit inside the remaining block space)
        # Example: If block is 64 bytes and we start at byte 60, max length is 4.
        max_len = L1DCACHE_BLOCK_SIZE - b_pos
        num_bytes = random.randint(1, max_len)

        # D. Append to lists
        target_sets.append(s)
        target_ways.append(w)
        target_byte_positions.append(b_pos)
        target_lengths.append(num_bytes)
        random_values.append(random.randint(0, 255))

        # Print verification for this specific injection
        print(f"Inj #{i+1} @ Tick {random_ticks[i]}: "
            f"Set={s:2d}, Way={w:1d}, Byte={b_pos:2d}, Len={num_bytes:2d}")

    system.injector = FaultInjector(
        inject_ticks=random_ticks,  
        target_sets=target_sets,
        target_ways=target_ways,
        target_byte_positions=target_byte_positions,
        target_lengths=target_lengths,
        # random_values=random_values,
        dump_cache_content=config["dump_cache_content"],
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
