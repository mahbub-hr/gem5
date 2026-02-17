from m5.params import *
from m5.SimObject import SimObject


class FaultInjector(SimObject):
    type = "FaultInjector"
    cxx_header = "fault_injector/fault_injector.hh"
    cxx_class = "gem5::FaultInjector"

    inject_tick = Param.Tick(0, "Time before the event.")
    target_address = Param.Addr(0, "The address to inject faults into.")
    target_bit = Param.Int(0, "The position to flip.")
    target_object = Param.SimObject("the object to inject faults into")
