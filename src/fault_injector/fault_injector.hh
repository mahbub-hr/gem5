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
    Tick injectTick;
    Addr targetAddress;
    int targetBit;
    Cache *targetCache;

    void processEvent();
    EventFunctionWrapper injectEvent;
};
} // namespace gem5

#endif // __FAULT_INJECTOR_HH__
