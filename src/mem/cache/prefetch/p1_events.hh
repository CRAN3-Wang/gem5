// Typed T2/P1 cooperation. No algorithm may manufacture SubmissionContext.
#ifndef __MEM_CACHE_PREFETCH_P1_EVENTS_HH__
#define __MEM_CACHE_PREFETCH_P1_EVENTS_HH__
#include "mem/cache/prefetch/instruction_state.hh"
namespace gem5::prefetch {
struct RetiredOperation {
    uint64_t sequence = 0;
    ContextID context = 0;
    Addr pc = 0, address = 0, value = 0;
    MicroPC microPC = 0;
    InstructionSource source;
    RequestPtr request;
    unsigned width = 0;
    bool load = false, propagate = false, reset = false;
    bool buffered = false; // forwarded/assembled ReadReq completion buffer
    std::vector<unsigned> sources, addressSources, destinations;
};
struct LoadIdentity : public Extension<Request, LoadIdentity> {
    uint64_t sequence;
    MicroPC microPC;
    LoadIdentity(uint64_t seq, MicroPC upc) : sequence(seq), microPC(upc) {}
    std::unique_ptr<ExtensionBase> clone() const override {
        return std::make_unique<LoadIdentity>(*this);
    }
};
struct PointerStream {
    // T2 must change version on replacement/reset; slot alone is never identity.
    uint64_t slot = 0, version = 0;
    Addr pc = 0;
    MicroPC microPC = 0;
    ContextID context = 0;
    InstructionSource source;
    bool operator==(const PointerStream &s) const {
        return slot == s.slot && version == s.version && pc == s.pc &&
            microPC == s.microPC && context == s.context && source == s.source;
    }
};
struct ProducerRead {
    PointerStream stream;
    // Exact translated producer byte address, not an aligned line. Translation
    // and security come from T2's real request, never guessed from its VA.
    RequestPtr translated;
};
}
#endif
