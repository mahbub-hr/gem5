#ifndef __FAULT_INJECTOR_HH__
#define __FAULT_INJECTOR_HH__

#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "base/types.hh"
#include "params/BaseFaultInjector.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

namespace gem5
{
class BaseCPU;
class ThreadContext;

class BaseFaultInjector : public SimObject
{
  public:
    BaseFaultInjector(const BaseFaultInjectorParams &p);
    void startup() override;
    void rebindCpu(BaseCPU *newCpu);

  protected:
    struct AppliedRecord
    {
        size_t index;
        Tick tick;
        bool landed;
    };

    std::vector<Tick> injectionSchedule;
    std::map<Tick, std::vector<size_t>> tickToPoints;
    std::vector<Tick> distinctTicks;
    size_t currentTickIndex = 0;

    std::vector<Counter> instSchedule;
    std::map<Counter, std::vector<size_t>> instToPoints;
    std::vector<Counter> distinctInsts;
    size_t currentInstIndex = 0;
    bool instMode = false;
    BaseCPU *cpu = nullptr;
    ThreadContext *armedTc = nullptr;

    int flipsApplied = 0;
    std::string resultFile;
    std::string domainName;
    std::string targetComponent;
    std::string selectionMode;
    std::vector<AppliedRecord> appliedLog;

    virtual void
    selectLocations(Tick t, std::vector<size_t> &pts)
    {}
    virtual bool applyFault(size_t pointIndex) = 0;
    virtual void writePointLocation(std::ostream &s, size_t i) const = 0;
    virtual void
    afterTick(Tick t)
    {}

  private:
    void processEvent();
    void processInstEvent();
    void armInstEvent(Counter target);
    void applyPointsAt(const std::vector<size_t> &pts, Tick logTick);
    void writeResult();
    bool resultWritten = false;
    EventFunctionWrapper injectEvent;
    EventFunctionWrapper injectInstEvent;
};
} // namespace gem5

#endif // __FAULT_INJECTOR_HH__
