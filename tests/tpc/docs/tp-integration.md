# Real T2/P1 integration review

Integration base: `624c030a68` on the team's `tpc-project`, including the T2
merge (`8bbbf52ac7`), P1 merge (`93a7f21671`) and initial connector commit.

## Production binding

`TPCController.bindHierarchy` is the common binding point used by the parent
project's frozen system. With source 1 enabled, it creates a CPU-owned
`TPCCPUAdapter` for load-address, completion, fetch RAS, committed branch and
squash facts. With source 1 or 2, it binds the O3 retirement observer; T2
announces streams before P1 receives the corresponding committed value. T2
also receives the CPU MMU for deferred producer-address translations.

The initial merge never instantiated the adapter, leaving real T2 inactive in
the production profiles. Checking SimObject availability or running P1 with its
test producer could not detect that gap.

## Connector corrections

- T2's issue/retire join supplies the exact retiring sequence together with its
  selected RAS stream. P1 consumes that announcement once, rather than choosing
  another registered stream with the same PC/micro-PC. Missing, replayed,
  different-width/address or different-loop pending identities cannot seed P1.
- Asynchronous translations retain a work generation. Squash/reset advances
  it, so a later valid issue cannot revive an older callback's capability.
  Stream versions and instruction residency are checked independently.
- Canceling one stream discards its obsolete subscriptions without processing
  other ready subscriptions in the middle of a batch invalidation.
- A producer stride deviation revokes its pointer contract and handle. T2's
  existing frozen stride classification is preserved; pointer eligibility is
  suppressed until SIT replacement or loop rebinding. Distance feedback is
  refreshed on the next issue. These are conservative coverage limits.

`tests/tpc/connector.py` exercises real T2/P1 instances with directed identities:
two RAS contexts at one PC, unmatched retirement, stale translation completion
after squash and a fresh issue, a live completion, and stride revocation.
The existing P1 directed fixture also covers cancellation of two ready streams.
These checks need an opt-in `TPC_TESTS=1` build; they complement the production
guest runs below.

```sh
TPC_TESTS=1 JOBS=1 gem5/tests/tpc/build-p1.sh PROTOCOL=None USE_SYSTEMC=0
# Supply the matching Python shared-library directory if your installation needs it.
gem5/build/X86/gem5.fast -d gem5/build/tp-connector \
    gem5/tests/tpc/connector.py gem5/build/tp-validation/pointer
gem5/build/p1-venv/bin/python gem5/tests/tpc/validate_p1.py \
    --output gem5/build/tp-p1-fixture-validation
```

Rebuild without `TPC_TESTS` before normal experiments.

## Proposal review

Reviewed against the two-page project proposal, especially IV (Proposed
Solution), V-B (Functional Validation) and the integration milestone, and
ISCA 2018 *Division of Labor*, IV-A/B/D.

| Requirement | Integration contract |
| --- | --- |
| T2 uses loop timing and strides, targets L1 | O3 adapter feeds actual CPU facts; T2 owns stride prediction, catch-up and L1 source-1 submissions. |
| P1 connects strided values to dependent addresses | T2 publishes versioned streams; P1 uses explicit x86 address-register provenance and four consistent signed offsets. |
| Second-stage requests follow returned pointer data | Future producer VAs are translated, then P1 reads exact eight-byte resident data or subscribes to an L1 fill. Dependent pointer-plus-offset VAs use P1-owned translating submissions to L1, source 2. |
| Coordinator routes components | Existing T2/P1/C1 priority and source accounting remain authoritative. Retirement supplies P1 training independently of ordinary cache-policy fallthrough. |
| Producer distance covers two serial reads | P1 reports recognition; T2 applies a factor of two to its base distance once. |
| Combined-system functional validation | `validate_tp.py` uses real components from the project factory, native checksum comparisons, positive/negative/zero offsets, varying-offset and overwritten-register controls, and accounting conservation. |

P1's committed-order provenance trains later than the paper's decode-time TPU.
This supports the proposal's array-of-pointers scope; self-dependent recursive
pointer chasing is not claimed. Supported scope remains single-thread X86 SE
with stable process mappings. Instruction classifications add no modeled lookup
latency; table/timeout bounds and unsupported load forms are described in
[P1 design](p1.md). Functional checks do not establish the proposal's full
SPEC/CRONO/NPB performance results.

## Reproduction

From the parent project root, using Python 3.10–3.12 with SCons:

```sh
JOBS=1 gem5/tests/tpc/build-p1.sh PROTOCOL=None USE_SYSTEMC=0
gem5/build/p1-venv/bin/python gem5/tests/tpc/validate_tp.py
```

The runner compiles `tests/tpc/pointer.c` statically and runs the real frozen
system through `tests/tpc/t2.py`. It requires the enclosing project's
`configs/system.py` and `configs/prefetchers.py`, like the existing focused T2
and P1 drivers. Artifacts default to `gem5/build/tp-validation/`; per-case logs,
configurations, guest output and native statistics remain available there.
No `TPC_TESTS` build or fixture producer is used.

## Validation evidence (2026-11-19)

The Python 3.11/GCC 16 X86 production build passed with `PROTOCOL=None
USE_SYSTEMC=0`. All nine production cases passed native checksum equality,
expected pointer offsets and readiness, exact retiring-stream trace matching,
idempotent distance feedback, and accepted/outcome conservation for T2/P1/C1.
The baseline, T2-only and C1-only cases generated no P1 candidates; varying and
overwritten-register TP controls also generated none.

The positive TP guest exercised 8,114 future producer translations, 7,118
resident subscriptions, 996 deferred subscriptions and 994 delivered values.
It generated 16,222 P1 candidates; 8,110 were accepted and timely useful.
Source-1 fills delivered producer bytes. The full TPC positive case also passed
these checks. These are functional-path observations, not a workload speedup
claim. In the independent-register production control T2 did not confirm a
stream; its stronger dependency-rejection check is the isolated P1 fixture,
where a test producer explicitly announces the stream.

The 20 parent host tests, native pointer guest's seven modes, and standalone
T2 engine and P1 arithmetic tests also passed. Runtime artifacts and the
machine-readable production report are under `build/tp-validation/`.

The opt-in X86 build also passed. The real-component directed connector test
and extended P1 directed test passed, followed by all seven P1 guest modes
(positive, negative, zero, varying, overwritten-register, split and forwarded).
Those regressions check native checksums, expected PCs/offsets/readiness,
resident/deferred/pending return paths, buffered split/forwarded completions,
negative controls and accounting conservation. Artifacts are in
`build/tp-connector/`, `build/tp-connector.log` and
`build/tp-p1-fixture-validation/`.
