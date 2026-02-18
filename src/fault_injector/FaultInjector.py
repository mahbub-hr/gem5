from m5.params import *
from m5.SimObject import SimObject


class FaultInjector(SimObject):
    type = "FaultInjector"
    cxx_header = "fault_injector/fault_injector.hh"
    cxx_class = "gem5::FaultInjector"

    inject_ticks = VectorParam.Tick([], "A list of ticks at which to inject faults.")
    target_object = Param.SimObject("the object to inject faults into")
    dump_cache_content = Param.Bool(False, "Whether to dump the cache content before and after injection.")
    target_sets = VectorParam.UInt32([], "The list of cache sets to inject faults into.")
    target_ways = VectorParam.UInt32([], "The list of cache ways to inject faults into.")
    target_byte_positions = VectorParam.UInt32([], "The list of byte positions within the cache block to inject faults into.")
    target_lengths = VectorParam.UInt32([], "The list of number of bytes to inject faults into starting from byte_pos.")