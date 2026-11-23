#ifndef __FAULT_INJECTOR_ALLOCATION_TRACER_HH__
#define __FAULT_INJECTOR_ALLOCATION_TRACER_HH__

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/types.hh"
#include "params/AllocationTracer.hh"
#include "sim/probe/probe_listener_object.hh"

namespace gem5
{

class BaseCPU;

class AllocationTracer : public ProbeListenerObject
{
  public:
    AllocationTracer(const AllocationTracerParams &params);

    void startup() override;
    void regProbeListeners() override;
    void checkPc(const Addr &pc);

  private:
    struct PendingCall
    {
        size_t function;
        Addr arg0;
        Addr arg1;
        Addr lr;
        uint64_t entryInst;
    };

    void writeEnd();
    void writeEvent(const PendingCall &call, Addr ret, uint64_t returnInst);

    BaseCPU *cpu;
    std::string traceFile;
    std::vector<std::string> functionNames;
    std::unordered_map<Addr, size_t> entryToFunction;
    std::vector<PendingCall> stack;
    std::ofstream trace;
    uint64_t sequence = 0;
    Addr prevX0;
    Addr prevX1;
    Addr prevX30;
    bool listening = false;
};

} // namespace gem5

#endif // __FAULT_INJECTOR_ALLOCATION_TRACER_HH__
