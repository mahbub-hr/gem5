#include "fault_injector/fault_injector.hh"
#include "debug/FI.hh" // You need to define this debug flag

#include <iostream>

namespace gem5
{
FaultInjector::FaultInjector(const FaultInjectorParams &p)
    : SimObject(p),
      injectTick(p.inject_tick),
      targetAddress(p.target_address),
      targetBit(p.target_bit),
      // We cast the generic SimObject param to a Cache pointer
      targetCache(dynamic_cast<Cache *>(p.target_object)),
      injectEvent([this] { processEvent(); }, name())
{
    std::cout << "Hello world! From a simobject " << std::endl;
    if (!targetCache) {
        fatal("FaultInjector: target_object is not a Cache!");
    }
}

void
FaultInjector::startup()
{
    // Schedule the event only if the tick is in the future
    if (injectTick > curTick()) {
        schedule(injectEvent, injectTick);
    }
}

void
FaultInjector::processEvent()
{
    DPRINTF(FI,
                "FaultInjector: Trying dcache bit flip at %#x on Tick %lu\n",
                targetAddress, curTick());

    bool success = targetCache->corruptStoredBlock(targetAddress, targetBit);

    if (success) {
        DPRINTF(FI,
                "FaultInjector: SRAM Bit Flip injected at %#x on Tick %lu\n",
                targetAddress, curTick());
    } else {
        DPRINTF(FI,
                "FaultInjector: Missed! Address %#x was not in cache at Tick "
                "%lu\n",
                targetAddress, curTick());
    }
}
} // namespace gem5
