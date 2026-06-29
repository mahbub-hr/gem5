#include "fault_injector/fault_injector.hh"

#include <iostream>

#include "base/output.hh"
#include "sim/sim_exit.hh"

namespace gem5
{
BaseFaultInjector::BaseFaultInjector(const BaseFaultInjectorParams &p)
    : SimObject(p),
      injectionSchedule(p.inject_ticks),
      resultFile(p.result_file),
      domainName(p.domain),
      targetComponent(p.target_component),
      selectionMode("preselected"),
      injectEvent([this] { processEvent(); }, name())
{
    for (size_t i = 0; i < injectionSchedule.size(); i++) {
        tickToPoints[injectionSchedule[i]].push_back(i);
    }
    for (const auto &entry : tickToPoints) {
        distinctTicks.push_back(entry.first);
    }
    registerExitCallback([this] { writeResult(); });
}

void
BaseFaultInjector::startup()
{
    if (!distinctTicks.empty()) {
        Tick firstTick = distinctTicks[0];
        if (firstTick > curTick()) {
            schedule(injectEvent, firstTick);
        }
    }
}

void
BaseFaultInjector::processEvent()
{
    Tick t = distinctTicks[currentTickIndex];
    std::vector<size_t> pts = tickToPoints[t];
    selectLocations(t, pts);
    for (size_t i : pts) {
        bool landed = applyFault(i);
        appliedLog.push_back({i, t, landed});
        if (landed) {
            flipsApplied++;
        }
    }
    afterTick(t);

    currentTickIndex++;
    if (currentTickIndex < distinctTicks.size()) {
        schedule(injectEvent, distinctTicks[currentTickIndex]);
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
