#ifndef __MEM_CACHE_PREFETCH_P1_HH__
#define __MEM_CACHE_PREFETCH_P1_HH__
#include <array>
#include <optional>
#include "mem/cache/prefetch/framework.hh"
#include "mem/cache/prefetch/p1_math.hh"
#include "sim/eventq.hh"
namespace gem5 {
struct P1PrefetcherParams;
namespace prefetch {
class P1Prefetcher : public TPCComponent {
    friend class StateFixture;
    struct Token {
        PointerStream stream;
        uint64_t sequence, value;
    };
    struct Relation {
        bool valid = false;
        uint64_t epoch = 0, lastProducer = 0;
        PointerStream stream;
        InstructionSource dependent;
        Addr pc = 0;
        MicroPC microPC = 0;
        OffsetConfidence confidence;
        std::optional<SubmissionContext> capability;
    };
    struct Pending {
        size_t slot;
        uint64_t epoch;
        Addr physical = 0, value = 0;
        bool secure = false, returned = false;
        Tick ready = 0, expires = 0;
    };
    const unsigned threshold, streamLimit, pendingLimit;
    const Cycles computationLatency;
    const Tick timeout;
    const bool trace;
    uint64_t epoch = 0;
    ContextID currentContext = InvalidContextID;
    std::array<std::optional<Token>, 256> registers;
    std::vector<PointerStream> streams;
    std::vector<Relation> relations;
    std::vector<Pending> pending;
    EventFunctionWrapper wake;
    struct Stats : statistics::Group {
        statistics::Scalar samples, trained, mismatches, returns, overflow,
            canceled, unsupported, predictions, demandValues, residentValues, deferredValues;
        Stats(statistics::Group *);
    } p1Stats;
    bool active(const PointerStream &) const;
    void cancel(const PointerStream &);
    void clearRelation(Relation &);
    void updateClassification(const InstructionSource &);
    void scheduleWake();
    void process();
    void queue(size_t, Addr, bool, bool, Addr, Tick);
    void consume(const FillEvent &);
    std::optional<Token> dependency(const std::vector<unsigned> &) const;
  protected:
    bool canHandle(const AccessEvent &) const override;
    void onPredict(const AccessEvent &, const SubmissionContext &) override {}
    void onRetiredOperation(const RetiredOperation &, const SubmissionContext *) override;
    void onPointerStream(const PointerStream &) override;
    void onProducerRead(const ProducerRead &) override;
    void onPointerStreamCanceled(const PointerStream &s) override { cancel(s); }
    bool isPointerStream(const PointerStream &) const override;
    void onFill(const FillEvent &e) override { consume(e); }
    void onInstructionInvalidated(const InstructionSource &) override;
  public:
    explicit P1Prefetcher(const P1PrefetcherParams &);
};
}
}
#endif
