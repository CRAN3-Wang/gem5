// Directed production-cache tests. This translation unit is opt-in only.
#include "mem/cache/prefetch/state_fixture.hh"
#include "params/TPCStateFixture.hh"
#include "mem/cache/prefetch/p1.hh"
#include "params/TPCFixtureComponent.hh"
#include "sim/core.hh"
namespace gem5::prefetch {
FixtureComponent::FixtureComponent(const TPCFixtureComponentParams &p)
    : TPCComponent(p), recognized(p.recognize) {}
StateFixture::StateFixture(const TPCStateFixtureParams &p)
    : SimObject(p), cache(p.cache), dataCache(p.data_cache),
      controller(p.controller), component(p.component) {}
void StateFixture::run() {
    unsigned block = cache->getBlockSize();
    auto fill = [&](Addr address, bool secure = false) {
        auto req = std::make_shared<Request>(address, block,
            Request::INST_FETCH | (secure ? Request::SECURE : 0), Request::funcRequestorId);
        Packet packet(req, MemCmd::ReadResp);
        packet.allocate();
        std::memset(packet.getPtr<uint8_t>(), 0, block);
        PacketList writebacks;
        auto *blk = cache->handleFill(&packet, nullptr, writebacks, true);
        for (auto *pkt : writebacks) delete pkt;
        return InstructionSource{cache, address, blk->instructionState->generation, 7, secure};
    };
    auto original = fill(0x100000);
    if (dynamic_cast<P1Prefetcher *>(component)) { runP1(original); return; }
    auto read = [&](InstructionSource s, int c) { return cache->readInstructionState(s,c); };
    auto update = [&](InstructionSource s, int c, int from, int to) {
        return cache->updateInstructionState(s,c,from,to);
    };
    panic_if(read(original,3) != 0 || !update(original,3,0,1), "Initial state");
    panic_if(read(original,1) != 0 || read(original,2) != 0, "Namespace isolation");
    auto otherOffset = original; otherOffset.offset++;
    panic_if(read(otherOffset,3) != 0, "Offset isolation");
    // Hit response captures the old source before a later invalidation.
    auto fetchReq = std::make_shared<Request>(original.line + 7, 8,
        Request::INST_FETCH, Request::funcRequestorId);
    Packet fetchPacket(fetchReq, MemCmd::ReadReq); fetchPacket.allocate();
    auto *blk = cache->tags->findBlock(original.line,false);
    cache->satisfyRequest(&fetchPacket,blk);
    auto inFlight = instructionSource(fetchReq);
    panic_if(!(inFlight == original), "Fetch response provenance");
    auto copiedReq = std::make_shared<Request>(*fetchReq);
    panic_if(!(instructionSource(copiedReq) == original), "Copied/split request provenance");

    if (auto *c1 = dynamic_cast<C1Prefetcher *>(component)) {
        // Exercise cleanup through the real synchronous cache notification.
        c1->tables.allocate(0,original,0x400007,0);
        c1->tables.observe(0x200000,64,false);
        c1->tables.associate(original,0x400007,0,0x200000,64,false);
        cache->invalidateBlock(blk);
        panic_if(c1->tables.im[0].valid || c1->tables.rm[0].instructions,
                 "I-cache invalidation must clear IM/RM references");
    } else {
        cache->invalidateBlock(blk);
    }
    auto reloaded = fill(original.line);
    panic_if(reloaded.generation == original.generation || read(reloaded,3) != 0,
             "Reload must create unknown state in new generation");
    panic_if(cache->isSourceValid(inFlight) || update(original,3,0,3),
             "Old dynamic instruction or delayed response must not update reload");
    auto secured = fill(original.line,true);
    panic_if(!update(secured,3,0,2) || read(reloaded,3) != 0, "Security isolation");
    // Move the block with the production tag-store API; identity follows content.
    CacheBlk *destination = nullptr;
    cache->tags->forEachBlk([&](CacheBlk &candidate) {
        if (!candidate.isValid() && !destination && candidate.getSet() ==
            cache->tags->findBlock(reloaded.line,false)->getSet()) destination = &candidate;
        return false;
    });
    panic_if(!destination, "No empty destination for move test");
    cache->tags->moveBlock(cache->tags->findBlock(reloaded.line,false),destination);
    panic_if(!cache->isSourceValid(reloaded), "Block movement lost residency");
    // Explicit replacement follows the same evictBlock -> invalidateBlock path.
    PacketList writebacks;
    cache->evictBlock(cache->tags->findBlock(reloaded.line,false),writebacks);
    for(auto *pkt:writebacks) delete pkt;
    panic_if(cache->isSourceValid(reloaded), "Replacement must invalidate state");
    reloaded = fill(original.line);
    // A stale request is ignored, then a live one uses the actual controller.
    auto request = std::make_shared<Request>(0x200000,8,0,Request::funcRequestorId,0x400007,0);
    request->setPaddr(0x400000);
    setInstructionSource(request,original);
    AccessEvent event; event.request=request; event.level=1; event.address=0x400000; event.primaryMiss=true;
    controller->cacheAccess(event);
    panic_if(read(reloaded,3)!=0, "Stale access recreated classification");
    setInstructionSource(request,reloaded);
    controller->cacheAccess(event);
    if(auto *fake=dynamic_cast<FixtureComponent *>(component)) {
        panic_if(fake->source()!=1 || fake->trains!=1 || fake->predicts!=1,
                 "Ordered ownership must stop on recognition without candidates");
        panic_if(cache->accounting->value("trainingEvents",2) ||
                 cache->accounting->value("trainingEvents",3), "Lower sources trained after ownership");
        panic_if(fake->deferred()!=SubmissionResult::Transport ||
                 fake->deferred()!=SubmissionResult::Transport, "Live deferred transport failed");
        panic_if(cache->accounting->value("dropQueueDuplicate",1)!=1,
                 "Duplicate queued prediction was not counted");
        event.address=0x600000;
        controller->cacheAccess(event);
        panic_if(cache->accounting->value("dropDemandCancel",1)!=1,
                 "Demand cancellation was not counted");
        cache->invalidateBlock(cache->tags->findBlock(reloaded.line,false));
        fill(reloaded.line);
        panic_if(fake->deferred()!=SubmissionResult::ControllerRejected,
                 "Deferred prediction from stale residency was admitted");
    } else {
        panic_if(read(reloaded,3)!=1, "Live primary miss must start C1 monitoring");
        auto *c1=dynamic_cast<C1Prefetcher *>(component);
        // Real observe/train wrappers complete 4 dense evicted regions.
        for(unsigned region=0;region<24;++region) {
            for(unsigned k=0;k<7;++k) {
                event.address=0x400000+region*1024+k*64;
                controller->cacheAccess(event);
            }
        }
        panic_if(read(reloaded,3)!=3, "C1 dense training did not complete");
        panic_if(!c1 || c1->tables.freeSlot()!=0, "Completed IM was not released");
        // Re-fetch and retrain the same PC on a sparse stream.
        cache->invalidateBlock(cache->tags->findBlock(reloaded.line,false));
        reloaded = fill(original.line);
        setInstructionSource(request,reloaded);
        for(unsigned region=0;region<24;++region) {
            for(unsigned k=0;k<2;++k) {
                event.address=0x800000+region*1024+k*64;
                controller->cacheAccess(event);
            }
        }
        panic_if(read(reloaded,3)!=2, "Sparse retraining must classify non-dense");
        // beginPhase/reset must leave resident classification intact.
        cache->accounting->beginPhase("fixture-measurement");
        panic_if(read(reloaded,3)!=2, "Phase transition cleared classification");
    }
    inform("TPC_STATE_FIXTURE_PASS");
}
}

namespace gem5::prefetch {
void StateFixture::runP1(const InstructionSource &source) {
    auto *p1 = dynamic_cast<P1Prefetcher *>(component);
    auto req = std::make_shared<Request>(0x200000, 8, 0,
        Request::funcRequestorId, 0x400007, 0);
    req->setPaddr(0x300000);
    req->setExtension(std::make_shared<LoadIdentity>(1, 0));
    setInstructionSource(req, source);
    AccessEvent access;
    access.level=1; access.size=8; access.address=0x300000;
    access.request=req; access.primaryMiss=true;
    controller->cacheAccess(access); // actual T2 ownership plus explicit handoff
    panic_if(p1->streams.size()!=1, "T2 ownership starved P1 handoff");
    auto stream=p1->streams[0];
    auto dependent=source; dependent.offset++;
    auto producer = [&](uint64_t seq, Addr value, unsigned reg) {
        RetiredOperation e;
        e.sequence=seq; e.context=0; e.pc=0x400007; e.source=source;
        e.request=req; e.load=true; e.width=8; e.value=value;
        e.destinations={reg};
        controller->retiredOperation(e);
    };
    auto consumer = [&](uint64_t seq, Addr address, unsigned reg) {
        RetiredOperation e;
        e.sequence=seq; e.context=0; e.pc=0x400008; e.source=dependent;
        e.request=std::make_shared<Request>(*req);
        setInstructionSource(e.request, dependent);
        e.load=true; e.width=8; e.address=address;
        e.addressSources={reg}; e.destinations={20};
        controller->retiredOperation(e);
    };
    for (unsigned i=0;i<4;++i) {
        producer(10+i*2, 0x600000+i*64, 10);
        consumer(11+i*2, 0x600008+i*64, 10);
        panic_if(p1->isPointerStream(stream)!=(i==3), "Four observations required");
    }
    panic_if(cache->readInstructionState(dependent,2)!=2 ||
        cache->readInstructionState(dependent,1)!=0 ||
        cache->readInstructionState(dependent,3)!=0, "P1 state namespace");
    // Multiple in-flight instances survive independently in architectural registers.
    producer(30,0x700000,10); producer(31,0x800000,11);
    consumer(32,0x700008,10); consumer(33,0x800008,11);
    panic_if(p1->relations[0].confidence.delta!=8 ||
             p1->relations[0].confidence.samples!=4, "Dynamic producer confusion");
    // Source2 candidates must not be submitted at completion/packet arrival.
    panic_if(cache->accounting->value("generated",2), "P1 ignored compute readiness");
    // Overwrite kills taint even if the new value numerically resembles a pointer.
    RetiredOperation overwrite; overwrite.context=0; overwrite.destinations={10};
    controller->retiredOperation(overwrite);
    auto samples=p1->p1Stats.samples.value();
    consumer(34,0x700008,10);
    panic_if(p1->p1Stats.samples.value()!=samples, "Overwrite did not kill dependency");
    producer(40,0x900000,10); consumer(41,0x8ffff8,10);
    panic_if(p1->isPointerStream(stream) || p1->relations[0].confidence.samples!=1,
             "Offset mismatch did not reset confidence");
    for(unsigned i=0;i<3;++i) { producer(42+i*2,0x900000,10); consumer(43+i*2,0x8ffff8,10); }
    panic_if(!p1->isPointerStream(stream), "Negative offset failed to retrain");
    // Two independent exact pointer offsets in one line, with a modeled fill.
    p1->pending.clear();
    for(unsigned i=0;i<2;++i) {
        auto read=std::make_shared<Request>(0x201000+i*8,8,0,
            Request::funcRequestorId,stream.pc,0);
        read->setPaddr(0x301000+i*8);
        p1->onProducerRead({stream,read});
    }
    panic_if(p1->pending.size()!=2, "Multiple consumers per line lost");
    FillEvent fill{1,curTick(),0x301000,std::vector<uint8_t>(64),nullptr};
    fill.ready=curTick()+1000;
    uint64_t values[]={0xa00008,0xb00008};
    for(unsigned k=0;k<2;++k) for(unsigned j=0;j<8;++j)
        fill.data[k*8+j]=values[k]>>(j*8);
    fill.secure=true; controller->cacheFill(fill);
    panic_if(p1->pending[0].returned, "Security domain mixed");
    fill.secure=false; controller->cacheFill(fill);
    for(unsigned i=0;i<2;++i)
        panic_if(!p1->pending[i].returned || p1->pending[i].value!=values[i] ||
                 p1->pending[i].ready<=fill.ready, "Return offset/readiness incorrect");
    // Resident-cache path uses the actual cache block and a delayed array read.
    panic_if(!dataCache, "P1 fixture requires data_cache");
    auto residentReq=std::make_shared<Request>(0x302000,64,0,Request::funcRequestorId);
    Packet residentPacket(residentReq,MemCmd::ReadResp); residentPacket.allocate();
    std::memcpy(residentPacket.getPtr<uint8_t>(),fill.data.data(),64);
    PacketList writebacks;
    auto *dataBlock=dataCache->handleFill(&residentPacket,nullptr,writebacks,true);
    for(auto *packet:writebacks) delete packet;
    auto residentRead=std::make_shared<Request>(0x202008,8,0,Request::funcRequestorId,stream.pc,0);
    residentRead->setPaddr(0x302008);
    p1->onProducerRead({stream,residentRead});
    panic_if(p1->pending.back().value!=values[1] || !p1->pending.back().returned ||
             p1->pending.back().ready<=dataBlock->getWhenReady(), "Resident read timing/offset");
    auto count=p1->pending.size();
    auto splitRead=std::make_shared<Request>(0x20203c,8,0,Request::funcRequestorId,stream.pc,0);
    splitRead->setPaddr(0x30203c);
    p1->onProducerRead({stream,splitRead});
    panic_if(p1->pending.size()!=count, "Unsupported future split was accepted");
    // Reuse a full relation table while old slot-zero work is outstanding.
    auto oldEpoch=p1->relations[0].epoch;
    for(unsigned i=0;i<p1->relations.size();++i) {
        dependent.offset=source.offset+2+i;
        producer(100+i*2,0xc00000+i*64,10);
        consumer(101+i*2,0xc00008+i*64,10);
    }
    panic_if(p1->relations[0].epoch==oldEpoch, "Full relation table did not replace oldest slot");
    for(const auto &work:p1->pending)
        panic_if(work.slot==0 && work.epoch==oldEpoch, "Stale relation-slot work survived reuse");
    // Reusing a T2 slot revokes the old relation, registers and pending work.
    auto replacement=stream; replacement.version++;
    p1->onPointerStream(replacement);
    panic_if(!p1->pending.empty() || p1->isPointerStream(stream), "Stale stream survives replacement");
    p1->onProducerRead({stream,residentRead});
    panic_if(!p1->pending.empty(), "Stale producer return recreated work");
    // Invalidation is delivered synchronously by the real cache.
    cache->invalidateBlock(cache->tags->findBlock(source.line,false));
    panic_if(!p1->streams.empty(), "I-cache invalidation retained stream");
    inform("P1_DIRECTED_PASS");
}
}
