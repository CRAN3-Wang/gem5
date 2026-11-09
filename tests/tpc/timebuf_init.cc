// Pipeline records mix scalar counts with nontrivial pointer members.
// Verify recycling starts scalar lifetimes with zero initialization.
#include "base/refcnt.hh"
#include "cpu/timebuf.hh"
struct Record { unsigned count; gem5::RefCountingPtr<gem5::RefCounted> instruction[12]; };
int main()
{
    gem5::TimeBuffer<Record> buffer(1, 1);
    for (int round = 0; round < 32; ++round) {
        auto &slot = *buffer.getWire(1);
        if (slot.count != 0 || slot.instruction[0]) return 1;
        slot.count = 123;
        slot.instruction[0] = new gem5::RefCounted;
        buffer.advance();
    }
}
