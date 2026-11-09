#ifndef __MEM_CACHE_PREFETCH_P1_MATH_HH__
#define __MEM_CACHE_PREFETCH_P1_MATH_HH__
#include <cstdint>
#include <limits>
namespace gem5::prefetch {
inline bool pointerDelta(uint64_t address, uint64_t value, int64_t &delta) {
    __int128 d = __int128(address) - __int128(value);
    if (d < INT64_MIN || d > INT64_MAX) return false;
    delta = int64_t(d); return true;
}
inline bool pointerTarget(uint64_t value, int64_t delta, uint64_t &target) {
    if (!value) return false;
    __int128 sum = __int128(value) + delta;
    // Supported SE x86-64 user addresses (48-bit canonical, lower half).
    if (sum <= 0 || sum >= (__int128(1) << 47)) return false;
    target = uint64_t(sum); return true;
}
struct OffsetConfidence {
    int64_t delta = 0;
    unsigned samples = 0;
    bool observe(int64_t d, unsigned threshold) {
        if (!samples || delta != d) { delta = d; samples = 1; }
        else if (samples < threshold) ++samples;
        return samples >= threshold;
    }
};
}
#endif
