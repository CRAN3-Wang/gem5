// Opt-in T2 stimulus only. Never registered as a production T2Prefetcher.
#ifndef __MEM_CACHE_PREFETCH_P1_FIXTURE_HH__
#define __MEM_CACHE_PREFETCH_P1_FIXTURE_HH__
#include "mem/cache/prefetch/framework.hh"
namespace gem5 {
struct P1ProducerFixtureParams;
namespace prefetch {
class P1ProducerFixture : public TPCComponent {
    Addr producerPC;
  protected:
    void onRetiredOperation(const RetiredOperation &e,
                            const SubmissionContext *) override {
        if (e.load && e.pc == producerPC && instructionState(e.source) >= 0)
            announcePointerStream({0, e.source.generation, e.pc, e.microPC,
                                   e.context, e.source});
    }
    bool canHandle(const AccessEvent &e) const override {
        return !e.write && e.request->getPC() == producerPC;
    }
    void onPredict(const AccessEvent &e, const SubmissionContext &cap) override {
        auto id = e.request->getExtension<LoadIdentity>();
        if (!id || !e.request->hasVaddr() || e.size != 8) return;
        PointerStream stream{0, instructionSource(e.request).generation,
            producerPC, id->microPC, e.request->contextId(), instructionSource(e.request)};
        announcePointerStream(stream);
        unsigned distance = 4 * pointerDistanceMultiplier(stream);
        Addr offset = distance * 8;
        // Fixture limits predictions to the same virtual page; this PA is
        // derived from the demand's real translation, not a guessed mapping.
        if ((e.request->getVaddr() & 4095) + offset + 8 > 4096) return;
        auto req = std::make_shared<Request>(e.request->getVaddr() + offset, 8,
            e.request->getFlags(), e.request->requestorId(), producerPC,
            e.request->contextId());
        req->setPaddr(e.address + offset);
        requestPointerValue({stream, req}); // subscribe BEFORE submit/snoop
        submitCandidates(cap, {{req->getPaddr(), 1}});
    }
  public:
    unsigned multiplier(const PointerStream &s) const {
        return pointerDistanceMultiplier(s);
    }
    explicit P1ProducerFixture(const P1ProducerFixtureParams &);
};
}
}
#endif
