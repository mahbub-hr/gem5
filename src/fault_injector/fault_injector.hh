#ifndef __FAULT_INJECTOR_HH__
#define __FAULT_INJECTOR_HH__

#include "mem/cache/cache.hh"
#include "params/FaultInjector.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

namespace gem5
{
class FaultInjector : public SimObject
{
  public:
    FaultInjector(const FaultInjectorParams &p);
    void startup() override;

  private:
    size_t currentInjectionIndex;
    std::vector<Tick> injectionSchedule;
    std::vector<uint32_t> sets;
    std::vector<uint32_t> ways;
    std::vector<uint32_t> bytePositions;
    std::vector<uint32_t> byteMasks;
    Cache *targetCache;
    bool dumpCacheContent = false;

    void processEvent();
    EventFunctionWrapper injectEvent;
};
} // namespace gem5

#endif // __FAULT_INJECTOR_HH__
