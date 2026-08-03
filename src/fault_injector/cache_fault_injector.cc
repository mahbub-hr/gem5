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
CacheFaultInjector::applyFault(size_t i, ResolvedSite &site)
{
    site.kind = "memory";
    DPRINTF(FI, "CacheFaultInjector: set=%d, way=%d, byte_pos=%d, mask=%d\n",
            sets[i], ways[i], bytePositions[i], byteMasks[i]);
    Addr paddr = 0;
    bool success =
        targetCache->MBU(sets[i], ways[i], bytePositions[i], byteMasks[i], &paddr);
    if (success) {
        site.hasPaddr = true;
        site.paddr = paddr;
        if (resolveFaults) {
            Addr vaddr = 0;
            if (resolveVirtualAddress(paddr, vaddr)) {
                site.hasVaddr = true;
                site.vaddr = vaddr;
            } else {
                site.vaddrReason = "no_se_mapping";
            }
        } else {
            site.vaddrReason = "resolution_disabled";
        }
        DPRINTF(FI, "CacheFaultInjector: Success! paddr=%#x\n", paddr);
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

bool
CacheFaultInjector::armActivationWatch(size_t i)
{
    const ResolvedSite &site = resolvedSites[i];
    if (!site.hasPaddr) {
        return false;
    }
    targetCache->watchFaultAddress(
        site.paddr,
        [this, i](Packet *pkt) { noteSiteAccess(i, pkt); },
        [this, i](bool dataPreserved) { noteSiteEviction(i, dataPreserved); });
    DPRINTF(FI, "CacheFaultInjector: watching paddr=%#x for activation\n",
            site.paddr);
    return true;
}

void
CacheFaultInjector::afterTick(Tick t)
{
    if (dumpCacheContent) {
        targetCache->dumpCacheContent();
    }
}
} // namespace gem5
