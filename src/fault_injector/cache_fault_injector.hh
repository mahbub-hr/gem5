#ifndef __CACHE_FAULT_INJECTOR_HH__
#define __CACHE_FAULT_INJECTOR_HH__

#include <cstdint>
#include <ostream>
#include <vector>

#include "fault_injector/fault_injector.hh"
#include "mem/cache/cache.hh"
#include "params/CacheFaultInjector.hh"

namespace gem5
{
class CacheFaultInjector : public BaseFaultInjector
{
  public:
    CacheFaultInjector(const CacheFaultInjectorParams &p);

  protected:
    bool applyFault(size_t i) override;
    void writePointLocation(std::ostream &s, size_t i) const override;
    void afterTick(Tick t) override;

  private:
    Cache *targetCache;
    std::vector<uint32_t> sets;
    std::vector<uint32_t> ways;
    std::vector<uint32_t> bytePositions;
    std::vector<uint32_t> byteMasks;
    bool dumpCacheContent = false;
};
} // namespace gem5

#endif // __CACHE_FAULT_INJECTOR_HH__
