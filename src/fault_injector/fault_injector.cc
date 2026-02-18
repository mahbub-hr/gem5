#include "fault_injector/fault_injector.hh"
#include "debug/FI.hh" // You need to define this debug flag

#include <iostream>

namespace gem5
{
FaultInjector::FaultInjector(const FaultInjectorParams &p)
    : SimObject(p),
      injectionSchedule(p.inject_ticks),
      currentInjectionIndex(0),
      targetCache(dynamic_cast<Cache *>(p.target_object)),
      dumpCacheContent(p.dump_cache_content),
      sets(p.target_sets),
      ways(p.target_ways),
      bytePositions(p.target_byte_positions),
      lengths(p.target_lengths),
      injectEvent([this] { processEvent(); }, name())
{
    if (!targetCache) {
        fatal("FaultInjector: target_object is not a Cache!");
    }
    std::sort(injectionSchedule.begin(), injectionSchedule.end());
}

void
FaultInjector::startup()
{
    // Schedule the FIRST event (if the list isn't empty)
    if (!injectionSchedule.empty()) {
        Tick firstTick = injectionSchedule[0];
        
        if (firstTick > curTick()) {
            schedule(injectEvent, firstTick);        }
    }
}

void
FaultInjector::processEvent()
{
    DPRINTF(FI,
                "FaultInjector: set=%d, way=%d, byte_pos=%d, length=%d\n",
                sets[currentInjectionIndex],
                ways[currentInjectionIndex],
                bytePositions[currentInjectionIndex],
                lengths[currentInjectionIndex]);
    
    bool success = targetCache->MBU(
            sets[currentInjectionIndex], 
            ways[currentInjectionIndex], 
            bytePositions[currentInjectionIndex], 
            lengths[currentInjectionIndex]
        );

    if(dumpCacheContent){
        targetCache->dumpCacheContent();
    }

    if (success) {
        DPRINTF(FI,
                "FaultInjector: Success!\n"
                );
    } else {
        DPRINTF(FI,
                "FaultInjector: Missed!\n"
                );
    }

    // 3. SCHEDULE THE NEXT EVENT
    currentInjectionIndex++; // Move to next item in list

    // Check if there are more injections left
    if (currentInjectionIndex < injectionSchedule.size()) {
        Tick nextTick = injectionSchedule[currentInjectionIndex];
        // already sorted, so just take the next one
        schedule(injectEvent, nextTick);
        DPRINTF(FI, "FaultInjector: Next fault scheduled for Tick %lu\n", nextTick);
        
    } else {
        DPRINTF(FI, "FaultInjector: All injections completed.\n");
    }
}
} // namespace gem5
