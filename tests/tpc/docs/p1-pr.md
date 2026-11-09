P1 now recognizes an arrays-of-pointers producer/dependent load pair from actual
X86/O3 register provenance. Four consistent signed offsets enable source-2
prefetches to L1D using returned pointer values and context-correct virtual
translation through the existing transport/accounting pipeline.

The implementation adds bounded/versioned relations, retirement-time dynamic
producer identities, successful load-hit/miss/forwarded/split demand values,
exact-byte future producer subscriptions, readiness timing and cancellation.
Member B gets typed stream ownership, translated producer-read handoff and an
idempotent 1/2 lookahead multiplier. Real T2 remains absent; opt-in fixtures are
excluded from the normal production build and experimental profiles.

Retirement-time training deliberately approximates the paper's decode taint.
Memory/spill taint, linked-list traversal, future split producer subscriptions,
SMT/hot checkpoints and cache read-port contention are unsupported. The change
also fixes an O3 TimeBuffer initialization bug exposed by GCC 16: a focused
regression fails before value initialization and passes afterward.

Validation by the implementation author:

- Python 3.11.18/GCC16 X86 builds pass with and without `TPC_TESTS=1`
  (`PROTOCOL=None USE_SYSTEMC=0`); production fixture exclusion checked.
- Real gem5 directed tests and seven pointer modes pass native checksums,
  expected load PCs/offsets, readiness and accounting conservation. Positive
  mode exercises resident, miss and existing-MSHR producer return paths;
  varying/independent controls generate zero P1 candidates. Split/forwarded
  modes show buffered demand completions. Forwarded mode accepts zero
  prefetches and is not evidence of timeliness benefit.
- All seven production baseline/C1/naive-multi parent-worker scenarios pass,
  including C1 residency/eviction and 2M warmup/5M measurement.
- All 20 unchanged parent host tests, native guest semantics, arithmetic,
  existing C1 tables and the TimeBuffer regression pass.

Exact artifacts, binary hashes, commands, limitations and baseline/C1 results
are in `tests/tpc/docs/p1-validation.md`; integration contract is in
`tests/tpc/docs/p1.md`. The guest uses bounded static arrays after the host's
static libc allocator cleanup also failed under no-prefetch SE execution.
Independent simulator validation is in progress; this draft does not claim
independent approval or full TP/TPC validation. No parent repository files or
submodule pin are committed by this PR.
