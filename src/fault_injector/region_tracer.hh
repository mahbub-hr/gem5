#ifndef __FAULT_INJECTOR_REGION_TRACER_HH__
#define __FAULT_INJECTOR_REGION_TRACER_HH__

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "base/types.hh"
#include "params/RegionTracer.hh"
#include "sim/probe/probe_listener_object.hh"

namespace gem5
{

class BaseCPU;

class RegionTracer : public ProbeListenerObject
{
  public:
    RegionTracer(const RegionTracerParams &params);

    void startup() override;
    void regProbeListeners() override;
    void notePc(const Addr &pc);

  private:
    bool inProgram(Addr pc) const;
    void writeEnd();

    BaseCPU *cpu;
    std::string traceFile;
    std::vector<std::pair<Addr, Addr>> ranges;
    std::ofstream trace;
    uint64_t index = 0;
    uint64_t programInsts = 0;
    uint64_t libraryInsts = 0;
    bool haveRegion = false;
    bool lastProgram = false;
    bool listening = false;
};

} // namespace gem5

#endif // __FAULT_INJECTOR_REGION_TRACER_HH__
