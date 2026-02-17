import m5
from m5.objects import *

config = {
    "inject_tick": 8983355000,
    "target_address": 0xb4cd0,
    "target_bit":128,
    "target_component": "dcache",
    # run from /work/host/gem5/configs/fault_injector
    "cmd": "/work/host/gem5/configs/fault_injector/tests/verify_fi/verify_fi.bin",
}


system = System()
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MiB")]
system.cpu = TimingSimpleCPU()
system.cpu.isa = [X86ISA() for i in range(system.cpu.numThreads)]

system.cpu.dcache = Cache(
    size="8KiB",
    assoc=2,
    tag_latency=2,
    data_latency=2,
    response_latency=2,
    mshrs=4,
    tgts_per_mshr=20,
)

system.injector = FaultInjector(
    inject_tick=config["inject_tick"], target_address=config["target_address"], target_bit=config["target_bit"], target_object=system.cpu.dcache
)

# A. Create the Bus FIRST
system.membus = SystemXBar()

# B. Connect CPU Instruction Port directly to Bus (since we have no I-Cache)
system.cpu.icache_port = system.membus.cpu_side_ports

# C. Connect CPU Data Port to Cache, and Cache to Bus
system.cpu.dcache.cpu_side = system.cpu.dcache_port
system.cpu.dcache.mem_side = system.membus.cpu_side_ports

# D. Connect Interrupts (x86 requirement)
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# 6. Memory Controller
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports
system.system_port = system.membus.cpu_side_ports

# The CPU needs a process to run, even if we are just testing the config.
# We create a dummy process "stub" so gem5 initializes the threads correctly.

system.workload = SEWorkload.init_compatible(config["cmd"])
process = Process()
process.cmd = [config["cmd"]]  # Dummy command
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system=False, system=system)
m5.instantiate()
print("Beginning simulation!")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
