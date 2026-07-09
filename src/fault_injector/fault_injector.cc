#include "fault_injector/fault_injector.hh"

#include <algorithm>
#include <iostream>

#include "base/output.hh"
#include "cpu/base.hh"
#include "cpu/thread_context.hh"
#include "sim/sim_exit.hh"

namespace gem5
{
BaseFaultInjector::BaseFaultInjector(const BaseFaultInjectorParams &p)
    : SimObject(p),
      injectionSchedule(p.inject_ticks),
      instSchedule(p.inject_insts),
      cpu(p.cpu),
      resultFile(p.result_file),
      domainName(p.domain),
      targetComponent(p.target_component),
      selectionMode("preselected"),
      injectEvent([this] { processEvent(); }, name()),
      injectInstEvent([this] { processInstEvent(); }, name())
{
    if (!injectionSchedule.empty() && !instSchedule.empty()) {
        fatal("BaseFaultInjector: inject_ticks and inject_insts are "
              "mutually exclusive; set exactly one.\n");
    }
    instMode = !instSchedule.empty();
    if (instMode && !cpu) {
        fatal("BaseFaultInjector: inject_insts requires the 'cpu' param.\n");
    }

    for (size_t i = 0; i < injectionSchedule.size(); i++) {
        tickToPoints[injectionSchedule[i]].push_back(i);
    }
    for (const auto &entry : tickToPoints) {
        distinctTicks.push_back(entry.first);
    }

    for (size_t i = 0; i < instSchedule.size(); i++) {
        instToPoints[instSchedule[i]].push_back(i);
    }
    for (const auto &entry : instToPoints) {
        distinctInsts.push_back(entry.first);
    }

    registerExitCallback([this] { writeResult(); });
}

void
BaseFaultInjector::startup()
{
    if (instMode) {
        if (!distinctInsts.empty()) {
            armInstEvent(distinctInsts[0]);
        }
        return;
    }
    if (!distinctTicks.empty()) {
        Tick firstTick = distinctTicks[0];
        if (firstTick > curTick()) {
            schedule(injectEvent, firstTick);
        }
    }
}

void
BaseFaultInjector::armInstEvent(Counter target)
{
    ThreadContext *tc = cpu->getContext(0);
    if (armedTc != nullptr && injectInstEvent.scheduled()) {
        armedTc->descheduleInstCountEvent(&injectInstEvent);
    }
    Tick targetTick = static_cast<Tick>(target);
    Tick now = tc->getCurrentInstCount();
    if (targetTick > now) {
        armedTc = tc;
        tc->scheduleInstCountEvent(&injectInstEvent, targetTick);
    } else {
        armedTc = nullptr;
        schedule(injectInstEvent, curTick());
    }
}

void
BaseFaultInjector::rebindCpu(BaseCPU *newCpu)
{
    cpu = newCpu;
    if (instMode && currentInstIndex < distinctInsts.size()) {
        armInstEvent(distinctInsts[currentInstIndex]);
    }
}

void
BaseFaultInjector::applyPointsAt(const std::vector<size_t> &ptsIn, Tick logTick)
{
    std::vector<size_t> pts = ptsIn;
    selectLocations(logTick, pts);
    for (size_t i : pts) {
        bool landed = applyFault(i);
        appliedLog.push_back({i, logTick, landed});
        if (landed) {
            flipsApplied++;
        }
    }
    afterTick(logTick);
}

void
BaseFaultInjector::processEvent()
{
    Tick t = distinctTicks[currentTickIndex];
    applyPointsAt(tickToPoints[t], t);

    currentTickIndex++;
    if (currentTickIndex < distinctTicks.size()) {
        schedule(injectEvent, distinctTicks[currentTickIndex]);
    }
}

void
BaseFaultInjector::processInstEvent()
{
    Counter c = distinctInsts[currentInstIndex];
    applyPointsAt(instToPoints[c], curTick());

    currentInstIndex++;
    if (currentInstIndex < distinctInsts.size()) {
        armInstEvent(distinctInsts[currentInstIndex]);
    }
}

void
BaseFaultInjector::writeResult()
{
    if (resultWritten) {
        return;
    }
    resultWritten = true;

    std::cout << "FaultInjector: flips_applied=" << flipsApplied << std::endl;

    OutputStream *os = simout.create(resultFile, false);
    if (!os || !os->stream()) {
        warn("BaseFaultInjector: could not create result file '%s'\n",
             resultFile);
        return;
    }
    std::ostream &s = *os->stream();

    s << "{\n";
    s << "  \"schema_version\": 1,\n";
    s << "  \"domain\": \"" << domainName << "\",\n";
    s << "  \"target_component\": \"" << targetComponent << "\",\n";
    s << "  \"selection_mode\": \"" << selectionMode << "\",\n";
    s << "  \"requested_points\": " << appliedLog.size() << ",\n";
    s << "  \"flips_applied\": " << flipsApplied << ",\n";

    bool first = true;
    s << "  \"applied\": [";
    for (const auto &e : appliedLog) {
        if (!e.landed) {
            continue;
        }
        s << (first ? "\n" : ",\n");
        first = false;
        s << "    {\"tick\": " << e.tick << ", ";
        writePointLocation(s, e.index);
        s << "}";
    }
    s << (first ? "" : "\n  ") << "],\n";

    first = true;
    s << "  \"missed\": [";
    for (const auto &e : appliedLog) {
        if (e.landed) {
            continue;
        }
        s << (first ? "\n" : ",\n");
        first = false;
        s << "    {\"tick\": " << e.tick << ", ";
        writePointLocation(s, e.index);
        s << ", \"reason\": \"not_applied\"}";
    }
    s << (first ? "" : "\n  ") << "]\n";
    s << "}\n";

    simout.close(os);
}
} // namespace gem5
