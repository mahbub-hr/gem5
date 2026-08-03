#include "fault_injector/fault_injector.hh"

#include <algorithm>
#include <iostream>

#include <memory>

#include "arch/generic/decoder.hh"
#include "arch/generic/pcstate.hh"
#include "base/loader/symtab.hh"
#include "base/output.hh"
#include "cpu/base.hh"
#include "cpu/static_inst.hh"
#include "cpu/thread_context.hh"
#include "mem/packet.hh"
#include "mem/page_table.hh"
#include "mem/se_translating_port_proxy.hh"
#include "sim/process.hh"
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
      resolveFaults(p.resolve_faults),
      trackActivation(p.track_activation),
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

    size_t numPoints = instMode ? instSchedule.size() : injectionSchedule.size();
    resolvedSites.resize(numPoints);

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
BaseFaultInjector::applyPointsAt(const std::vector<size_t> &ptsIn,
                                 Tick logTick)
{
    std::vector<size_t> pts = ptsIn;
    selectLocations(logTick, pts);
    for (size_t i : pts) {
        ResolvedSite site;
        bool landed = applyFault(i, site);
        site.landed = landed;
        if (landed) {
            captureInjectionContext(site);
        }
        resolvedSites[i] = site;
        appliedLog.push_back({i, logTick, landed});
        if (landed) {
            flipsApplied++;
            if (!trackActivation) {
                resolvedSites[i].endReason = "tracking_disabled";
            } else if (armActivationWatch(i)) {
                resolvedSites[i].watched = true;
                resolvedSites[i].live = true;
            } else {
                resolvedSites[i].endReason = "tracking_unsupported";
            }
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

bool
BaseFaultInjector::resolveVirtualAddress(Addr paddr, Addr &vaddr) const
{
    if (!cpu) {
        return false;
    }
    ThreadContext *tc = cpu->getContext(0);
    if (!tc) {
        return false;
    }
    Process *p = tc->getProcessPtr();
    if (!p || !p->pTable) {
        return false;
    }
    EmulationPageTable *pt = p->pTable;
    Addr pageBase = pt->pageAlign(paddr);
    Addr offset = pt->pageOffset(paddr);
    std::vector<std::pair<Addr, Addr>> mappings;
    pt->getMappings(&mappings);
    for (const auto &m : mappings) {
        if (m.second == pageBase) {
            vaddr = m.first + offset;
            return true;
        }
    }
    return false;
}

bool
BaseFaultInjector::currentPc(Addr &pc) const
{
    if (!cpu) {
        return false;
    }
    ThreadContext *tc = cpu->getContext(0);
    if (!tc) {
        return false;
    }
    pc = tc->pcState().instAddr();
    return true;
}

bool
BaseFaultInjector::currentInstCount(Counter &count) const
{
    if (!cpu) {
        return false;
    }
    ThreadContext *tc = cpu->getContext(0);
    if (!tc) {
        return false;
    }
    count = static_cast<Counter>(tc->getCurrentInstCount());
    return true;
}

bool
BaseFaultInjector::lookupSymbol(Addr addr, std::string &symbol)
{
    auto it = loader::debugSymbolTable.findNearest(addr);
    if (it == loader::debugSymbolTable.end()) {
        return false;
    }
    symbol = it->name();
    Addr offset = addr - it->address();
    if (offset != 0) {
        symbol += "+" + std::to_string(offset);
    }
    return true;
}

bool
BaseFaultInjector::disassembleAt(Addr pc, std::string &text) const
{
    if (!cpu) {
        return false;
    }
    ThreadContext *tc = cpu->getContext(0);
    if (!tc || !tc->getProcessPtr()) {
        return false;
    }
    InstDecoder *decoder = tc->getDecoderPtr();
    if (!decoder) {
        return false;
    }

    std::unique_ptr<PCStateBase> pcState(tc->pcState().clone());
    pcState->set(pc);

    SETranslatingPortProxy proxy(tc);
    size_t chunk = decoder->moreBytesSize();
    Addr fetchPc = pc & decoder->pcMask();

    decoder->reset();

    StaticInstPtr inst = nullptr;
    for (unsigned attempt = 0; attempt < maxDecodeChunks && !inst; attempt++) {
        if (!proxy.tryReadBlob(fetchPc, decoder->moreBytesPtr(), chunk)) {
            decoder->reset();
            return false;
        }
        decoder->moreBytes(*pcState, fetchPc);
        inst = decoder->decode(*pcState);
        fetchPc += chunk;
    }

    decoder->reset();

    if (!inst) {
        return false;
    }
    text = inst->disassemble(pc, &loader::debugSymbolTable);
    return true;
}

std::string
BaseFaultInjector::jsonEscape(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

void
BaseFaultInjector::captureInjectionContext(ResolvedSite &site) const
{
    site.injectTick = curTick();
    Addr pc = 0;
    if (currentPc(pc)) {
        site.hasInjectPc = true;
        site.injectPc = pc;
        lookupSymbol(pc, site.injectSymbol);
    }
    Counter count = 0;
    if (currentInstCount(count)) {
        site.hasInjectInst = true;
        site.injectInst = count;
    }
}

void
BaseFaultInjector::noteSiteAccess(size_t pointIndex, Packet *pkt)
{
    ResolvedSite &r = resolvedSites[pointIndex];
    if (!r.live) {
        return;
    }

    if (pkt->isRead()) {
        r.reads++;
        if (!r.activated) {
            r.activated = true;
            r.activationTick = curTick();
            r.activationSize = pkt->getSize();
            if (pkt->req) {
                if (pkt->req->hasPC()) {
                    r.hasActivationPc = true;
                    r.activationPc = pkt->req->getPC();
                    lookupSymbol(r.activationPc, r.activationSymbol);
                }
                if (pkt->req->hasVaddr()) {
                    r.hasActivationVaddr = true;
                    r.activationVaddr = pkt->req->getVaddr();
                }
            }
            Counter count = 0;
            if (currentInstCount(count)) {
                r.hasActivationInst = true;
                r.activationInst = count;
            }
        }
    }

    if (pkt->isWrite()) {
        r.writes++;
        r.live = false;
        r.endReason = "overwritten";
        r.hasEndTick = true;
        r.endTick = curTick();
    }
}

void
BaseFaultInjector::noteSiteEviction(size_t pointIndex, bool dataPreserved)
{
    ResolvedSite &r = resolvedSites[pointIndex];
    if (!r.live || dataPreserved) {
        return;
    }
    r.live = false;
    r.endReason = "evicted_clean";
    r.hasEndTick = true;
    r.endTick = curTick();
}

void
BaseFaultInjector::writeInjectionContext(std::ostream &s, size_t i) const
{
    const ResolvedSite &r = resolvedSites[i];
    if (r.hasInjectPc) {
        s << ", \"inject_pc\": " << r.injectPc;
    } else {
        s << ", \"inject_pc\": null";
    }
    if (!r.injectSymbol.empty()) {
        s << ", \"inject_symbol\": \"" << r.injectSymbol << "\"";
    } else {
        s << ", \"inject_symbol\": null";
    }
    if (!r.injectDisasm.empty()) {
        s << ", \"inject_disasm\": \"" << jsonEscape(r.injectDisasm) << "\"";
    } else {
        s << ", \"inject_disasm\": null";
    }
    if (r.hasInjectInst) {
        s << ", \"inject_inst_count\": " << r.injectInst;
    } else {
        s << ", \"inject_inst_count\": null";
    }
}

void
BaseFaultInjector::writeActivation(std::ostream &s, size_t i) const
{
    const ResolvedSite &r = resolvedSites[i];
    s << ", \"activation\": {\"watched\": " << (r.watched ? "true" : "false")
      << ", \"activated\": " << (r.activated ? "true" : "false");
    if (r.activated) {
        s << ", \"tick\": " << r.activationTick;
        s << ", \"delay_ticks\": " << (r.activationTick - r.injectTick);
    } else {
        s << ", \"tick\": null, \"delay_ticks\": null";
    }
    if (r.hasActivationPc) {
        s << ", \"pc\": " << r.activationPc;
    } else {
        s << ", \"pc\": null";
    }
    if (!r.activationSymbol.empty()) {
        s << ", \"symbol\": \"" << r.activationSymbol << "\"";
    } else {
        s << ", \"symbol\": null";
    }
    if (!r.activationDisasm.empty()) {
        s << ", \"disasm\": \"" << jsonEscape(r.activationDisasm) << "\"";
    } else {
        s << ", \"disasm\": null";
    }
    if (r.hasActivationInst) {
        s << ", \"inst_count\": " << r.activationInst;
    } else {
        s << ", \"inst_count\": null";
    }
    if (r.hasActivationInst && r.hasInjectInst) {
        s << ", \"delay_insts\": " << (r.activationInst - r.injectInst);
    } else {
        s << ", \"delay_insts\": null";
    }
    if (r.hasActivationVaddr) {
        s << ", \"vaddr\": " << r.activationVaddr;
    } else {
        s << ", \"vaddr\": null";
    }
    if (r.activated) {
        s << ", \"access_size\": " << r.activationSize;
    } else {
        s << ", \"access_size\": null";
    }
    s << ", \"reads\": " << r.reads << ", \"writes\": " << r.writes;
    if (r.endReason) {
        s << ", \"end_reason\": \"" << r.endReason << "\"";
    } else {
        s << ", \"end_reason\": null";
    }
    if (r.hasEndTick) {
        s << ", \"end_tick\": " << r.endTick;
    } else {
        s << ", \"end_tick\": null";
    }
    s << "}";
}

void
BaseFaultInjector::writeResolvedSite(std::ostream &s, size_t i) const
{
    const ResolvedSite &r = resolvedSites[i];
    s << ", \"kind\": \"" << r.kind << "\"";
    if (r.hasPaddr) {
        s << ", \"paddr\": " << r.paddr;
    } else {
        s << ", \"paddr\": null";
    }
    if (r.hasVaddr) {
        s << ", \"vaddr\": " << r.vaddr;
    } else {
        s << ", \"vaddr\": null";
        if (r.vaddrReason) {
            s << ", \"vaddr_reason\": \"" << r.vaddrReason << "\"";
        }
    }
}

void
BaseFaultInjector::writeResult()
{
    if (resultWritten) {
        return;
    }
    resultWritten = true;

    for (auto &r : resolvedSites) {
        if (r.hasInjectPc) {
            disassembleAt(r.injectPc, r.injectDisasm);
        }
        if (r.hasActivationPc) {
            disassembleAt(r.activationPc, r.activationDisasm);
        }
    }

    std::cout << "FaultInjector: flips_applied=" << flipsApplied << std::endl;

    OutputStream *os = simout.create(resultFile, false);
    if (!os || !os->stream()) {
        warn("BaseFaultInjector: could not create result file '%s'\n",
             resultFile);
        return;
    }
    std::ostream &s = *os->stream();

    s << "{\n";
    s << "  \"schema_version\": 3,\n";
    s << "  \"domain\": \"" << domainName << "\",\n";
    s << "  \"target_component\": \"" << targetComponent << "\",\n";
    s << "  \"selection_mode\": \"" << selectionMode << "\",\n";
    s << "  \"requested_points\": " << appliedLog.size() << ",\n";
    s << "  \"flips_applied\": " << flipsApplied << ",\n";

    int resolvedCount = 0;
    s << "  \"resolved_addresses\": [";
    bool firstAddr = true;
    for (const auto &r : resolvedSites) {
        if (r.landed && r.hasVaddr) {
            s << (firstAddr ? "" : ", ") << r.vaddr;
            firstAddr = false;
            resolvedCount++;
        }
    }
    s << "],\n";
    s << "  \"resolved_count\": " << resolvedCount << ",\n";

    int activatedCount = 0;
    for (const auto &r : resolvedSites) {
        if (r.activated) {
            activatedCount++;
        }
    }
    s << "  \"activated_count\": " << activatedCount << ",\n";

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
        writeResolvedSite(s, e.index);
        writeInjectionContext(s, e.index);
        writeActivation(s, e.index);
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
        writeResolvedSite(s, e.index);
        s << ", \"reason\": \"not_applied\"}";
    }
    s << (first ? "" : "\n  ") << "]\n";
    s << "}\n";

    simout.close(os);
}
} // namespace gem5
