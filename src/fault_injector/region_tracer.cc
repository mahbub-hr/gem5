#include "fault_injector/region_tracer.hh"

#include <algorithm>

#include "base/logging.hh"
#include "base/output.hh"
#include "cpu/base.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

RegionTracer::RegionTracer(const RegionTracerParams &p)
    : ProbeListenerObject(p), cpu(p.cpu), traceFile(p.trace_file)
{
    if (!cpu) {
        fatal("RegionTracer: the 'cpu' param is required.\n");
    }
    if (p.program_starts.size() != p.program_ends.size()) {
        fatal("RegionTracer: program_starts and program_ends differ in "
              "length.\n");
    }
    if (p.program_starts.empty()) {
        fatal("RegionTracer: no program ranges given.\n");
    }
    std::vector<std::pair<Addr, Addr>> sorted;
    for (size_t i = 0; i < p.program_starts.size(); i++) {
        if (p.program_starts[i] >= p.program_ends[i]) {
            fatal("RegionTracer: range %d has start >= end.\n", i);
        }
        sorted.emplace_back(p.program_starts[i], p.program_ends[i]);
    }
    std::sort(sorted.begin(), sorted.end());
    for (const auto &r : sorted) {
        if (!ranges.empty() && r.first <= ranges.back().second) {
            ranges.back().second = std::max(ranges.back().second, r.second);
        } else {
            ranges.push_back(r);
        }
    }
}

void
RegionTracer::startup()
{
    trace.open(simout.resolve(traceFile), std::ios::out | std::ios::trunc);
    if (!trace.is_open()) {
        fatal("RegionTracer: cannot open '%s'.\n", traceFile);
    }
    trace << "{\"format\": \"region_trace\", \"version\": 1}\n";
    listening = true;
    registerExitCallback([this] { writeEnd(); });
}

void
RegionTracer::regProbeListeners()
{
    typedef ProbeListenerArg<RegionTracer, Addr> RegionListener;
    connectListener<RegionListener>(this, "RetiredInstsPC",
                                    &RegionTracer::notePc);
}

bool
RegionTracer::inProgram(Addr pc) const
{
    auto it = std::upper_bound(ranges.begin(), ranges.end(), pc,
                               [](Addr value, const std::pair<Addr, Addr> &r) {
                                   return value < r.first;
                               });
    if (it == ranges.begin()) {
        return false;
    }
    --it;
    return pc < it->second;
}

void
RegionTracer::notePc(const Addr &pc)
{
    if (!listening) {
        return;
    }
    bool program = inProgram(pc);
    if (!haveRegion || program != lastProgram) {
        trace << index << ' ' << (program ? 'p' : 'l') << '\n';
        haveRegion = true;
        lastProgram = program;
    }
    if (program) {
        programInsts++;
    } else {
        libraryInsts++;
    }
    index++;
}

void
RegionTracer::writeEnd()
{
    if (!trace.is_open()) {
        return;
    }
    trace << "{\"end\": true, \"total_insts\": " << index
          << ", \"program_insts\": " << programInsts
          << ", \"library_insts\": " << libraryInsts << "}\n";
    trace.flush();
    trace.close();
}

} // namespace gem5
