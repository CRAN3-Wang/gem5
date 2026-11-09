#include <cassert>
#include <cstdint>
#include "mem/cache/prefetch/p1_math.hh"
using namespace gem5::prefetch;
int main() {
    OffsetConfidence c;
    for (int i=0;i<3;++i) assert(!c.observe(8,4));
    assert(c.observe(8,4)); assert(c.observe(8,4));
    assert(!c.observe(-8,4)); assert(c.samples==1);
    for(int i=0;i<100;++i) assert(!c.observe(i%2 ? 8 : -8,4));
    int64_t delta;
    assert(pointerDelta(0x1000,0x1008,delta) && delta==-8);
    assert(!pointerDelta(UINT64_MAX,0,delta));
    assert(pointerDelta(0,uint64_t(1)<<63,delta) && delta==INT64_MIN);
    uint64_t target;
    assert(pointerTarget(0x1008,-8,target) && target==0x1000);
    assert(pointerTarget(0x1ff8,8,target) && target==0x2000);
    assert(!pointerTarget(0,8,target));
    assert(!pointerTarget(1,-8,target));
    assert(!pointerTarget(UINT64_MAX,8,target));
    assert(!pointerTarget((uint64_t(1)<<47)-8,8,target));
    assert(!pointerTarget(uint64_t(1)<<63,INT64_MIN,target));
}
