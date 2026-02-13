#include "fault_injector/fault_injector.hh"
#include "debug/Faults.hh" // You need to define this debug flag

#include <iostream>

namespace gem5
{
FaultInjector::FaultInjector(const FaultInjectorParams &p)
    : SimObject(p),
      injectTick(p.inject_tick),
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
    // We assume you want to target a specific address (from your trace)
    // Or you can pick a random Set/Way if you prefer.

    // Example: Corrupt the Physical Address we care about (0x8d000)
    // You can make this a parameter: p.target_address
    Addr victimAddr = 0x8d000;

    bool success = targetCache->corruptStoredBlock(victimAddr, targetBit);

    if (success) {
        DPRINTF(Faults,
                "FaultInjector: SRAM Bit Flip injected at %#x on Tick %lu\n",
                victimAddr, curTick());
    } else {
        DPRINTF(Faults,
                "FaultInjector: Missed! Address %#x was not in cache at Tick "
                "%lu\n",
                victimAddr, curTick());
    }
}
} // namespace gem5
