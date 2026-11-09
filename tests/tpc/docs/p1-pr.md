Add Member C's arrays-of-pointers P1 and typed X86/O3 integration

P1 recognizes a dependent load only through register provenance from an actual
successful producer instance and four consistent offsets. Returned pointer
values generate source-2, L1D virtual candidates through the existing MMU,
transport, ownership checks and lifecycle accounting.

The change adds retirement-time X86/O3 provenance, explicit T2 stream/translation
handoff, P1-owned controller capabilities, exact-byte resident/fill subscriptions,
data-ready timing, bounded/versioned work and recovery/instruction invalidation.
It deliberately uses later retirement-time training instead of the paper's
decode-time taint. Future split producers, memory taint, linked-list traversal
and cache read-port contention are unsupported. Real T2 remains absent.

Validation: all changed translation units compile with the supported Python
3.11 environment; focused arithmetic and existing C1 tables pass. The unchanged parent
project has 20 passing host tests; the gem5-contained native guest test covers
seven pointer modes. Final
build/runtime evidence will be appended before publication. The opt-in producer
fixture is not an experimental T2 implementation.
