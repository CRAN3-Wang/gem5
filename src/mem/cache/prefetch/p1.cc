#include "mem/cache/prefetch/p1.hh"
#include <algorithm>
#include "params/P1Prefetcher.hh"
namespace gem5::prefetch {
P1Prefetcher::Stats::Stats(statistics::Group *p) : statistics::Group(p, "p1"),
    ADD_STAT(samples, statistics::units::Count::get(), "Dependent offset observations"),
    ADD_STAT(trained, statistics::units::Count::get(), "Relations reaching confidence"),
    ADD_STAT(mismatches, statistics::units::Count::get(), "Offset changes"),
    ADD_STAT(returns, statistics::units::Count::get(), "Exact pointer values returned"),
    ADD_STAT(overflow, statistics::units::Count::get(), "Bounded work rejected"),
    ADD_STAT(canceled, statistics::units::Count::get(), "Obsolete value work canceled"),
    ADD_STAT(unsupported, statistics::units::Count::get(), "Unsupported producer reads"),
    ADD_STAT(predictions, statistics::units::Count::get(), "Dependent candidates submitted"),
    ADD_STAT(demandValues, statistics::units::Count::get(), "Retired producer values"),
    ADD_STAT(residentValues, statistics::units::Count::get(), "Resident producer array reads"),
    ADD_STAT(deferredValues, statistics::units::Count::get(), "Producer reads waiting for fill") {}
P1Prefetcher::P1Prefetcher(const P1PrefetcherParams &p)
    : TPCComponent(p), threshold(p.consistent_observations),
      streamLimit(p.stream_entries), pendingLimit(p.pending_values),
      computationLatency(p.computation_latency), timeout(p.value_timeout),
      trace(p.trace), relations(p.relation_entries),
      wake([this] { process(); }, name() + ".values"), p1Stats(this) {
    fatal_if(source() != 2 || !threshold || !streamLimit || !pendingLimit ||
        relations.empty() || computationLatency < Cycles(1) || !timeout,
        "Invalid P1 parameters");
}
bool P1Prefetcher::active(const PointerStream &s) const {
    return std::find(streams.begin(), streams.end(), s) != streams.end() &&
           instructionState(s.source) >= 0;
}
void P1Prefetcher::updateClassification(const InstructionSource &source) {
    int state = instructionState(source);
    if (state < 0) return;
    int desired = 0;
    for (const auto &r : relations)
        if (r.valid && r.dependent == source)
            desired = std::max(desired, r.confidence.samples >= threshold ? 2 : 1);
    updateInstructionState(source, state, desired);
}
void P1Prefetcher::clearRelation(Relation &r) {
    auto dependent = r.dependent;
    bool valid = r.valid;
    r = Relation{};
    r.epoch = ++epoch;
    if (valid) updateClassification(dependent);
}
void P1Prefetcher::cancel(const PointerStream &s) {
    streams.erase(std::remove(streams.begin(), streams.end(), s), streams.end());
    for (auto &r : registers) if (r && r->stream == s) r.reset();
    for (auto &r : relations) if (r.valid && r.stream == s) clearRelation(r);
    process();
}
void P1Prefetcher::onPointerStream(const PointerStream &s) {
    if (active(s)) return;
    // Slot replacement invalidates old consumers before installing new identity.
    auto old = std::find_if(streams.begin(), streams.end(), [&](const auto &x) {
        return x.slot == s.slot && x.context == s.context;
    });
    if (old != streams.end()) { auto copy = *old; cancel(copy); }
    if (streams.size() == streamLimit) { auto copy = streams.front(); cancel(copy); }
    streams.push_back(s);
}
bool P1Prefetcher::isPointerStream(const PointerStream &s) const {
    for (const auto &r : relations)
        if (r.valid && r.stream == s && r.confidence.samples >= threshold) return true;
    return false;
}
bool P1Prefetcher::canHandle(const AccessEvent &e) const {
    auto identity = e.request->getExtension<LoadIdentity>();
    if (!identity) return false;
    for (const auto &r : relations)
        if (r.valid && r.dependent == instructionSource(e.request) &&
            r.stream.context == e.request->contextId() &&
            r.pc == e.request->getPC() && r.microPC == identity->microPC &&
            r.confidence.samples >= threshold) return true;
    return false;
}
std::optional<P1Prefetcher::Token>
P1Prefetcher::dependency(const std::vector<unsigned> &inputs) const {
    std::optional<Token> result;
    for (auto reg : inputs) {
        if (reg >= registers.size() || !registers[reg]) continue;
        auto t = registers[reg];
        if (!active(t->stream)) continue;
        // Mixed dynamic producers are ambiguous, even at the same PC.
        if (result && (result->sequence != t->sequence ||
                       !(result->stream == t->stream))) return {};
        result = t;
    }
    return result;
}
void P1Prefetcher::onRetiredOperation(const RetiredOperation &e,
                                     const SubmissionContext *capability) {
    if (e.reset) {
        for (auto &r : registers) r.reset();
        streams.clear();
        for (auto &r : relations) clearRelation(r);
        process();
        currentContext = InvalidContextID;
        return;
    }
    if (currentContext != e.context) {
        for (auto &r : registers) r.reset();
        // Supported system has one fixed SE process. Context change revokes work.
        if (currentContext != InvalidContextID) {
            streams.clear();
            for (auto &r : relations) clearRelation(r);
            process();
        }
        currentContext = e.context;
    }
    auto dep = dependency(e.load ? e.addressSources : e.sources);
    if (e.load && capability && dep && active(dep->stream)) {
        int64_t delta;
        if (pointerDelta(e.address, dep->value, delta)) {
            auto it = std::find_if(relations.begin(), relations.end(), [&](const auto &r) {
                return r.valid && r.stream == dep->stream && r.dependent == e.source &&
                       r.microPC == e.microPC && r.pc == e.pc;
            });
            if (it == relations.end()) {
                it = std::find_if(relations.begin(), relations.end(),
                                  [](const auto &r) { return !r.valid; });
                if (it == relations.end())
                    it = std::min_element(relations.begin(), relations.end(),
                        [](const auto &a, const auto &b) { return a.epoch < b.epoch; });
                clearRelation(*it);
                it->valid = true; it->stream = dep->stream; it->dependent = e.source;
                it->pc = e.pc; it->microPC = e.microPC;
            }
            if (it->lastProducer < dep->sequence) {
                it->lastProducer = dep->sequence;
                bool wasTrained = it->confidence.samples >= threshold;
                if (it->confidence.samples && it->confidence.delta != delta) {
                    ++p1Stats.mismatches;
                    wasTrained = false;
                    it->epoch = ++epoch; // all queued values used the previous offset
                }
                bool trained = it->confidence.observe(delta, threshold);
                it->capability = *capability;
                ++p1Stats.samples;
                if (trained && !wasTrained) ++p1Stats.trained;
                updateClassification(e.source);
                if (trace) inform("P1_RELATION producer=%#x dependent=%#x seq=%llu producer_seq=%llu delta=%lld samples=%u tick=%llu",
                    dep->stream.pc, e.pc, e.sequence, dep->sequence, delta,
                    it->confidence.samples, curTick());
            }
        }
    }
    std::optional<Token> output;
    if (e.load && capability && e.width == 8) {
        for (const auto &s : streams) {
            if (s.pc != e.pc || s.microPC != e.microPC || s.context != e.context ||
                !(s.source == e.source)) continue;
            output = Token{s, e.sequence, e.value};
            ++p1Stats.demandValues;
            if (trace) inform("P1_VALUE producer=%#x seq=%llu value=%#x buffered=%d tick=%llu",
                e.pc, e.sequence, e.value, e.buffered, curTick());
            for (size_t i = 0; i < relations.size(); ++i)
                if (relations[i].valid && relations[i].stream == s &&
                    relations[i].confidence.samples >= threshold)
                    queue(i, 0, false, true, e.value, curTick());
            break;
        }
    } else if (e.propagate) output = dep;
    // Every integer write overwrites provenance, including unsupported loads.
    for (auto reg : e.destinations)
        if (reg < registers.size()) registers[reg] = output;
    process();
}
void P1Prefetcher::queue(size_t slot, Addr physical, bool secure,
                         bool returned, Addr value, Tick ready) {
    if (pending.size() >= pendingLimit) { ++p1Stats.overflow; return; }
    pending.push_back({slot, relations[slot].epoch, physical, value, secure,
        returned, ready + clockPeriod() * computationLatency, curTick() + timeout});
}
void P1Prefetcher::onProducerRead(const ProducerRead &read) {
    const auto &req = read.translated;
    unsigned block = cacheView(1).blockSize();
    if (!active(read.stream) || !req || !req->hasPaddr() || !req->hasVaddr() ||
        !req->hasContextId() || req->contextId() != read.stream.context ||
        req->getSize() != 8 || req->isUncacheable() ||
        (req->getPaddr() % block) + 8 > block) {
        ++p1Stats.unsupported; return;
    }
    std::vector<uint8_t> bytes;
    Tick ready = 0;
    bool resident = pointerBytes(req->getPaddr(), req->isSecure(), bytes, ready);
    if (resident) ++p1Stats.residentValues; else ++p1Stats.deferredValues;
    Addr value = 0;
    if (resident)
        for (unsigned j = 0; j < 8; ++j)
            value |= uint64_t(bytes[req->getPaddr() % block + j]) << (8 * j);
    for (size_t i = 0; i < relations.size(); ++i)
        if (relations[i].valid && relations[i].stream == read.stream &&
            relations[i].confidence.samples >= threshold)
            queue(i, req->getPaddr(), req->isSecure(), resident, value, ready);
    scheduleWake();
}
void P1Prefetcher::consume(const FillEvent &e) {
    if (e.level != 1) return;
    for (auto &p : pending) {
        if (p.returned || p.secure != e.secure || p.physical < e.address) continue;
        Addr offset = p.physical - e.address;
        if (offset > e.data.size() || e.data.size() - offset < 8) continue;
        p.value = 0;
        for (unsigned i = 0; i < 8; ++i) p.value |= uint64_t(e.data[offset+i]) << (8*i);
        p.returned = true;
        p.ready = std::max(curTick(), e.ready) + clockPeriod() * computationLatency;
        ++p1Stats.returns;
    }
    scheduleWake();
}
void P1Prefetcher::scheduleWake() {
    Tick next = MaxTick;
    for (const auto &p : pending)
        next = std::min(next, p.returned ? std::min(p.ready, p.expires) : p.expires);
    if (wake.scheduled()) deschedule(wake);
    if (next != MaxTick) schedule(wake, std::max(curTick() + 1, next));
}
void P1Prefetcher::process() {
    for (auto it = pending.begin(); it != pending.end();) {
        auto &r = relations[it->slot];
        bool stale = !r.valid || r.epoch != it->epoch || !active(r.stream) ||
            instructionState(r.dependent) < 0 || curTick() >= it->expires;
        if (stale) { ++p1Stats.canceled; it = pending.erase(it); continue; }
        if (!it->returned || curTick() < it->ready) { ++it; continue; }
        Addr target;
        if (r.capability && pointerTarget(it->value, r.confidence.delta, target)) {
            submitCandidates(*r.capability, {{target, 1, 0, true}});
            ++p1Stats.predictions;
            if (trace) inform("P1_PREDICT producer=%#x dependent=%#x pointer=%#x delta=%lld va=%#x context=%d ready=%llu tick=%llu",
                r.stream.pc, r.pc, it->value, r.confidence.delta, target,
                r.stream.context, it->ready, curTick());
        }
        it = pending.erase(it);
    }
    scheduleWake();
}
void P1Prefetcher::onInstructionInvalidated(const InstructionSource &s) {
    auto copy = streams;
    for (const auto &stream : copy) if (stream.source.sameLine(s)) cancel(stream);
    for (auto &r : relations) if (r.valid && r.dependent.sameLine(s)) clearRelation(r);
    process();
}
}
