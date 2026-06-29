#include "fault_injector/cache_fault_injector.hh"

#include "debug/FI.hh"

namespace gem5
{
CacheFaultInjector::CacheFaultInjector(const CacheFaultInjectorParams &p)
    : BaseFaultInjector(p),
      targetCache(dynamic_cast<Cache *>(p.target_object)),
      sets(p.target_sets),
      ways(p.target_ways),
      bytePositions(p.target_byte_positions),
      byteMasks(p.target_byte_masks),
      dumpCacheContent(p.dump_cache_content)
{
    if (!targetCache) {
        fatal("CacheFaultInjector: target_object is not a Cache!");
    }
}

bool
CacheFaultInjector::applyFault(size_t i)
{
    DPRINTF(FI, "CacheFaultInjector: set=%d, way=%d, byte_pos=%d, mask=%d\n",
            sets[i], ways[i], bytePositions[i], byteMasks[i]);
    bool success =
        targetCache->MBU(sets[i], ways[i], bytePositions[i], byteMasks[i]);
    if (success) {
        DPRINTF(FI, "CacheFaultInjector: Success!\n");
    } else {
        DPRINTF(FI, "CacheFaultInjector: Missed!\n");
    }
    return success;
}

void
CacheFaultInjector::writePointLocation(std::ostream &s, size_t i) const
{
    s << "\"set\": " << sets[i] << ", \"way\": " << ways[i]
      << ", \"byte\": " << bytePositions[i] << ", \"mask\": " << byteMasks[i];
}

void
CacheFaultInjector::afterTick(Tick t)
{
    if (dumpCacheContent) {
        targetCache->dumpCacheContent();
    }
}
} // namespace gem5
