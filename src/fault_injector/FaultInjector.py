from m5.params import *
from m5.SimObject import SimObject


class BaseFaultInjector(SimObject):
    type = "BaseFaultInjector"
    abstract = True
    cxx_header = "fault_injector/fault_injector.hh"
    cxx_class = "gem5::BaseFaultInjector"

    inject_ticks = VectorParam.Tick(
        [], "A list of ticks at which to inject faults."
    )
    target_object = Param.SimObject("the object to inject faults into")
    result_file = Param.String(
        "injection_result.json",
        "Sidecar JSON result file written to the run output directory.",
    )
    domain = Param.String("base", "Domain tag emitted into the result file.")
    target_component = Param.String(
        "", "Component name emitted into the result file."
    )


class CacheFaultInjector(BaseFaultInjector):
    type = "CacheFaultInjector"
    cxx_header = "fault_injector/cache_fault_injector.hh"
    cxx_class = "gem5::CacheFaultInjector"

    target_sets = VectorParam.UInt32(
        [], "The list of cache sets to inject faults into."
    )
    target_ways = VectorParam.UInt32(
        [], "The list of cache ways to inject faults into."
    )
    target_byte_positions = VectorParam.UInt32(
        [],
        "The list of byte positions within the cache block to inject faults into.",
    )
    target_byte_masks = VectorParam.UInt32(
        [], "The list of byte masks to use for flipping bits."
    )
    dump_cache_content = Param.Bool(
        False, "Whether to dump the cache content before and after injection."
    )


class RegisterFaultInjector(BaseFaultInjector):
    type = "RegisterFaultInjector"
    cxx_header = "fault_injector/register_fault_injector.hh"
    cxx_class = "gem5::RegisterFaultInjector"

    target_registers = VectorParam.UInt32(
        [], "The list of architectural register indices to inject faults into."
    )
    target_byte_indices = VectorParam.UInt32(
        [], "The byte index within each register to flip."
    )
    target_byte_masks = VectorParam.UInt32(
        [], "The list of byte masks to use for flipping bits."
    )
