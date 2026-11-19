#include "mem/cache/prefetch/p1_fixture.hh"
#include "params/P1ProducerFixture.hh"
namespace gem5::prefetch {
P1ProducerFixture::P1ProducerFixture(const P1ProducerFixtureParams &p)
    : TPCComponent(p), producerPC(p.producer_pc) {}
}
