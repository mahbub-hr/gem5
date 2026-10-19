#include "fault_injector/fault_injector.hh"

#include <algorithm>
#include <sstream>

#include "base/loader/object_file.hh"
#include "base/output.hh"
#include "cpu/base.hh"
#include "cpu/reg_class.hh"
#include "cpu/thread_context.hh"
#include "mem/packet.hh"
#include "mem/page_table.hh"
#include "sim/process.hh"

namespace gem5
{
namespace
{
constexpr RegIndex x86FlatFramePointerIdx = 5;
constexpr RegIndex x86FlatStackPointerIdx = 4;
constexpr RegIndex arm64FlatFramePointerIdx = 32;
constexpr RegIndex arm64FlatStackPointerIdx = 38;
constexpr RegIndex armFlatFramePointerIdx = 11;
constexpr RegIndex thumbFlatFramePointerIdx = 7;
constexpr RegIndex armFlatStackPointerIdx = 13;
constexpr Addr addrMask32 = 0xffffffffULL;
} // namespace

BaseFaultInjector::BaseFaultInjector(const BaseFaultInjectorParams &p)
    : SimObject(p),
      injectionSchedule(p.inject_ticks),
      instSchedule(p.inject_insts),
      cpu(p.cpu),
      domainName(p.domain),
      targetComponent(p.target_component),
      selectionMode("preselected"),
      journalFile(p.journal_file),
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

    size_t numPoints =
        instMode ? instSchedule.size() : injectionSchedule.size();
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
}

void
BaseFaultInjector::startup()
{
    journal.open(simout.resolve(journalFile),
                 std::ios::out | std::ios::app);
    std::ostringstream armed;
    armed << "{\"event\":\"armed\",\"tick\":" << curTick()
          << ",\"requested_points\":" << resolvedSites.size()
          << ",\"domain\":\"" << jsonEscape(domainName)
          << "\",\"target_component\":\"" << jsonEscape(targetComponent)
          << "\",\"selection_mode\":\"" << jsonEscape(selectionMode)
          << "\",\"inst_mode\":" << (instMode ? "true" : "false") << "}";
    journalLine(armed.str());
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
    suspendTargetWatches(true);
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
        journalApplied(i, logTick);
        if (landed) {
            if (!trackActivation) {
                resolvedSites[i].endReason = "tracking_disabled";
                journalWatchEnded(i);
            } else if (armActivationWatch(i)) {
                resolvedSites[i].watched = true;
                if (!resolvedSites[i].activated) {
                    resolvedSites[i].live = true;
                }
            } else {
                resolvedSites[i].endReason = "tracking_unsupported";
                journalWatchEnded(i);
            }
        }
    }
    suspendTargetWatches(false);
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
BaseFaultInjector::currentFrameRegisters(Addr &fp, Addr &sp) const
{
    if (!cpu) {
        return false;
    }
    ThreadContext *tc = cpu->getContext(0);
    if (!tc) {
        return false;
    }
    Process *p = tc->getProcessPtr();
    if (!p || !p->objFile) {
        return false;
    }
    BaseISA *isa = tc->getIsaPtr();
    if (!isa) {
        return false;
    }
    const RegClass *intRC = isa->regClasses().at(IntRegClass);
    if (!intRC || !intRC->isFlat()) {
        return false;
    }

    loader::Arch arch = p->objFile->getArch();
    RegIndex fpIdx = 0;
    RegIndex spIdx = 0;
    switch (arch) {
        case loader::X86_64:
        case loader::I386:
            fpIdx = x86FlatFramePointerIdx;
            spIdx = x86FlatStackPointerIdx;
            break;
        case loader::Arm64:
            fpIdx = arm64FlatFramePointerIdx;
            spIdx = arm64FlatStackPointerIdx;
            break;
        case loader::Arm:
            fpIdx = armFlatFramePointerIdx;
            spIdx = armFlatStackPointerIdx;
            break;
        case loader::Thumb:
            fpIdx = thumbFlatFramePointerIdx;
            spIdx = armFlatStackPointerIdx;
            break;
        default:
            return false;
    }
    if (fpIdx >= intRC->numRegs() || spIdx >= intRC->numRegs()) {
        return false;
    }

    Addr mask = loader::archIs64Bit(arch) ? ~Addr(0) : addrMask32;
    fp = static_cast<Addr>(tc->getReg((*intRC)[fpIdx])) & mask;
    sp = static_cast<Addr>(tc->getReg((*intRC)[spIdx])) & mask;
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

std::string
BaseFaultInjector::hexAddr(Addr addr)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

void
BaseFaultInjector::captureInjectionContext(ResolvedSite &site) const
{
    site.injectTick = curTick();
    Addr pc = 0;
    if (!site.hasInjectPc && currentPc(pc)) {
        site.hasInjectPc = true;
        site.injectPc = pc;
    }
    Counter count = 0;
    if (currentInstCount(count)) {
        site.hasInjectInst = true;
        site.injectInst = count;
    }
    Addr fp = 0;
    Addr sp = 0;
    if (currentFrameRegisters(fp, sp)) {
        site.hasInjectFrame = true;
        site.injectFp = fp;
        site.injectSp = sp;
    }
}

void
BaseFaultInjector::noteSiteAccess(size_t pointIndex, Packet *pkt,
                                  bool isFunctional)
{
    ResolvedSite &r = resolvedSites[pointIndex];
    if (!r.live) {
        return;
    }

    if (pkt->isRead()) {
        r.reads++;
        if (!r.activated) {
            r.activated = true;
            r.activationKind = isFunctional ? "functional" : "load";
            r.activationTick = curTick();
            r.activationSize = pkt->getSize();
            if (!isFunctional && pkt->req) {
                if (pkt->req->hasPC()) {
                    r.hasActivationPc = true;
                    r.activationPc = pkt->req->getPC();
                }
                if (pkt->req->hasVaddr()) {
                    r.hasActivationVaddr = true;
                    r.activationVaddr = pkt->req->getVaddr();
                }
            }
            if (!r.hasActivationPc) {
                Addr pc = 0;
                if (currentPc(pc)) {
                    r.hasActivationPc = true;
                    r.activationPc = pc;
                }
            }
            Counter count = 0;
            if (currentInstCount(count)) {
                r.hasActivationInst = true;
                r.activationInst = count;
            }
            Addr fp = 0;
            Addr sp = 0;
            if (currentFrameRegisters(fp, sp)) {
                r.hasActivationFrame = true;
                r.activationFp = fp;
                r.activationSp = sp;
            }
            journalActivated(pointIndex);
        }
    }

    if (pkt->isWrite()) {
        r.writes++;
        r.live = false;
        r.endReason = isFunctional ? "overwritten_syscall" : "overwritten";
        r.hasEndTick = true;
        r.endTick = curTick();
        journalWatchEnded(pointIndex);
    }
}

void
BaseFaultInjector::noteSiteEviction(size_t pointIndex, bool dataPreserved)
{
    ResolvedSite &r = resolvedSites[pointIndex];
    if (!r.live) {
        return;
    }
    if (dataPreserved) {
        if (!r.writtenBack) {
            r.writtenBack = true;
            journalWrittenBack(pointIndex);
        }
        return;
    }
    if (r.writtenBack) {
        return;
    }
    r.live = false;
    r.endReason = "evicted_clean";
    r.hasEndTick = true;
    r.endTick = curTick();
    journalWatchEnded(pointIndex);
}

void
BaseFaultInjector::noteRegisterAccess(size_t pointIndex, bool isWrite)
{
    if (inNoteRegisterAccess) {
        return;
    }
    inNoteRegisterAccess = true;

    ResolvedSite &r = resolvedSites[pointIndex];
    if (!r.live) {
        inNoteRegisterAccess = false;
        return;
    }

    if (!isWrite) {
        r.reads++;
        if (!r.activated) {
            r.activated = true;
            r.activationKind = "register_read";
            r.activationTick = curTick();
            Addr pc = 0;
            if (currentPc(pc)) {
                r.hasActivationPc = true;
                r.activationPc = pc;
            }
            Counter count = 0;
            if (currentInstCount(count)) {
                r.hasActivationInst = true;
                r.activationInst = count;
            }
            Addr fp = 0;
            Addr sp = 0;
            if (currentFrameRegisters(fp, sp)) {
                r.hasActivationFrame = true;
                r.activationFp = fp;
                r.activationSp = sp;
            }
            journalActivated(pointIndex);
        }
    } else {
        r.writes++;
        r.live = false;
        r.endReason = "overwritten";
        r.hasEndTick = true;
        r.endTick = curTick();
        journalWatchEnded(pointIndex);
    }

    inNoteRegisterAccess = false;
}

void
BaseFaultInjector::journalLine(const std::string &line)
{
    if (!journal.is_open()) {
        return;
    }
    journal << line << '\n';
    journal.flush();
}

void
BaseFaultInjector::journalApplied(size_t i, Tick logTick)
{
    const ResolvedSite &r = resolvedSites[i];
    std::ostringstream s;
    s << "{\"event\":\"applied\",\"tick\":" << logTick
      << ",\"index\":" << i
      << ",\"landed\":" << (r.landed ? "true" : "false") << ", ";
    writePointLocation(s, i);
    writeResolvedSite(s, i);
    writeInjectionContext(s, i);
    s << "}";
    journalLine(s.str());
}

void
BaseFaultInjector::journalActivated(size_t i)
{
    const ResolvedSite &r = resolvedSites[i];
    std::ostringstream s;
    s << "{\"event\":\"activated\",\"tick\":" << r.activationTick
      << ",\"index\":" << i
      << ",\"kind\":\"" << jsonEscape(r.activationKind) << "\"";
    if (r.hasActivationPc) {
        s << ",\"pc\":" << r.activationPc;
    } else {
        s << ",\"pc\":null";
    }
    if (r.hasActivationInst) {
        s << ",\"inst_count\":" << r.activationInst;
    } else {
        s << ",\"inst_count\":null";
    }
    if (r.hasActivationVaddr) {
        s << ",\"vaddr\":" << r.activationVaddr;
    } else {
        s << ",\"vaddr\":null";
    }
    if (r.activationSize != 0) {
        s << ",\"access_size\":" << r.activationSize;
    } else {
        s << ",\"access_size\":null";
    }
    if (r.hasActivationFrame) {
        s << ",\"frame_address\":\"" << hexAddr(r.activationFp) << "\""
          << ",\"stack_pointer\":\"" << hexAddr(r.activationSp) << "\"";
    } else {
        s << ",\"frame_address\":null,\"stack_pointer\":null";
    }
    s << "}";
    journalLine(s.str());
}

void
BaseFaultInjector::journalWrittenBack(size_t i)
{
    std::ostringstream s;
    s << "{\"event\":\"written_back\",\"tick\":" << curTick()
      << ",\"index\":" << i << "}";
    journalLine(s.str());
}

void
BaseFaultInjector::journalWatchEnded(size_t i)
{
    const ResolvedSite &r = resolvedSites[i];
    std::ostringstream s;
    s << "{\"event\":\"watch_ended\",\"tick\":" << curTick()
      << ",\"index\":" << i
      << ",\"reason\":\"" << jsonEscape(r.endReason) << "\""
      << ",\"reads\":" << r.reads << ",\"writes\":" << r.writes << "}";
    journalLine(s.str());
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
    if (r.hasInjectInst) {
        s << ", \"inject_inst_count\": " << r.injectInst;
    } else {
        s << ", \"inject_inst_count\": null";
    }
    if (r.hasInjectFrame) {
        s << ", \"frame_address\": \"" << hexAddr(r.injectFp) << "\""
          << ", \"stack_pointer\": \"" << hexAddr(r.injectSp) << "\"";
    } else {
        s << ", \"frame_address\": null, \"stack_pointer\": null";
    }
}

void
BaseFaultInjector::writeResolvedSite(std::ostream &s, size_t i) const
{
    const ResolvedSite &r = resolvedSites[i];
    s << ", \"kind\": \"" << jsonEscape(r.kind) << "\"";
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
            s << ", \"vaddr_reason\": \""
              << jsonEscape(r.vaddrReason) << "\"";
        }
    }
}
} // namespace gem5
