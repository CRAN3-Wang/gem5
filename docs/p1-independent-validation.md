# Independent P1 validation

Status: **PASS for the exercised arrays-of-pointers P1 component scope**,
2026-11-09. No blocking implementation failure found. This is independent
runtime execution, not the earlier preliminary handoff or the author's runs.
Real T2 and integrated TP/TPC application/performance validation remain absent.

Only this report is committed by the validator. All generated helpers, guests,
logs and results are in `build/p1-independent-validation/` in the existing
implementation checkout. No builds, branch changes, parent source edits,
gitlink staging, pushes or PR creation were performed by the validator.

## Source and binary identities

Checkout: `/home/jwiki/.t3/worktrees/ECE552_project/t3-9360b91e/gem5`,
branch `member-c/p1-pointer`.

- Source/tests at start: `47aac84784e706ed7f81cd6406a316a884c05636`.
- Executable-source fix revision: `7ff5f09f63`; final guest/driver at `47aac84784`.
- Author's documentation-only HEAD during validation:
  `56f10526fdc021e792ad9d3c55e3f8823191c2f6`. No executable changes followed
  the stable handoff. Binary hashes were independently checked before and after
  execution; these are provided build artifacts, not independently rebuilt binaries.
- Production `build/X86/gem5.fast` SHA256:
  `3f0dfd4a6efc17dec9902025a72bff1da3e952a823970ea23d7307eb00792250`.
- Fixture `build/p1-validation/gem5-p1-fixture.fast` SHA256:
  `c2637d8fd0f47e4ca17bce397927a525f5c34aaea49dc706e99d96ff2267b12b`.
- Python: `build/p1-venv/bin/python`, Python 3.11.18;
  compiler: GCC 16.2.1 20260810. Handoff build options:
  `PROTOCOL=None USE_SYSTEMC=0`; production excludes `TPC_TESTS`, fixture uses 1.
- Guest SHA256: `16e9163ee25ff1d6a6227211d0efee7fb166f552f001a70e267b5e143fab48e1`;
  `tests/tpc/pointer.c`: `660b505bb3eec1bce73cc93a5bf3abf74c1dfdd217984ca8cc289f0984c654c9`.

`independent-audit.json` retains identities, final driver/helper hashes and
additional runtime assertions. Production embedded registration independently
passed: P1 exists, `T2Prefetcher`, `P1ProducerFixture` and `TPCStateFixture` do
not; T/TP/TPC factories reject missing T2 (`production-probe.log`). Initial
probe-helper errors (wrong exception type/profile spelling) were corrected only
in build-local helper code; they were not simulator defects.

## Acceptance criteria derived independently

Paper IV-B requires an actual address dependence from a strided producer load's
returned value to a later load, possibly through registers. Numerical address
correlation alone is insufficient. The constant signed offset must repeat for
four observations by default, with producer instances matched correctly and
unstable/overwritten dependencies rejected. Recognition doubles producer distance
once; returned pointer plus offset produces the dependent request. Paper IV-D
routes T2/P1 to L1, C1 to L2, and gives recognized ownership T2 then P1 then C1.
The project contract assigns P1 source 2 and target 1.

The proposal explicitly promises arrays of pointers. Paper self-dependent
linked-chain traversal is additional fidelity, not a substitute. Real T2 is
absent: test-only streams may exercise P1 but cannot establish integrated TP/TPC
application correctness or incremental performance. The paper's suite speedup is
not a correctness threshold for these microbenchmarks.

Table I provides hardware comparison targets (4-wide O3, 3 GHz, 192 ROB,
64 KiB L1, 256 KiB L2, 2 MiB L3, 64-byte lines, 4 GiB DDR3). Reuse the frozen
parent configuration and inspect actual config attachments rather than quietly
retuning it. Table II's one PtrPC, eight SIT entries, 64-bit TPU and 1.07 KiB P1
estimate are hardware-fidelity references; simulator metadata/retirement-time
provenance are documented approximations, not an equivalent area implementation.

Practical integration acceptance also requires real successful hit/miss/forwarded
and split demand values, finite/versioned deferred state, stale residency and
stream cancellation, correct context/security/translation, signed/page-crossing
address handling, modeled value readiness, common submission/accounting and
preserved C1/baselines. Resident snapshots omit cache-port contention by design;
future split producer subscriptions are explicitly rejected. Unsupported cases
must be reported, not counted as passes.

## Reproduction and artifacts

Commands below run from the gem5 checkout unless explicitly stated. Existing
artifact directories must be preserved; choose a fresh output path for reruns.

```sh
build/p1-venv/bin/python tests/tpc/validate_p1.py \
  --gem5 build/p1-validation/gem5-p1-fixture.fast \
  --output build/p1-independent-validation/simulator
```

The driver supplies Python's shared-library path, compiles its own static guest,
runs the real directed fixture and all seven modes (4096 entries, four rounds),
and compares native and simulated stdout. Artifacts:
`simulator/report.json`, `simulator/directed.log`, `simulator/MODE.log`,
`simulator/MODE/{config.json,stats.txt,benchmark.stdout}`, and top-level
`simulator-run.log`. Producer PC is `0x40343e`, dependent PC `0x40344a`,
resolved from this run's actual guest symbols.

Production regressions reused unchanged parent `benchmarks/run_micro.worker`,
with guest snapshots and helper exclusively inside gem5:

```sh
build/p1-venv/bin/python - <<'RUN'
import os, runpy, sys
from pathlib import Path
os.environ['LD_LIBRARY_PATH'] = str(Path(sys.base_prefix) / 'lib') + ':' + os.environ.get('LD_LIBRARY_PATH', '')
runpy.run_path('build/p1-independent-validation/run_regressions.py', run_name='__main__')
RUN
```

The prepared helper's ordinary `visit` nm match was corrected from uppercase
`T` to `[tT]` in build/ only. The helper requires new `regression-*` directories;
no completed case was needlessly rerun. `worker` launches the simulator from its
required parent cwd. Each folder retains source, compiled guest, input identity,
request, state, build metadata, config, simulation logs, phase report and
measurement-only statistics. Summary: `regressions.json`, progress:
`regressions-run.log`. No artifacts touch `build/p1-own-regressions`.

## Independent simulator results

| Mode | Predictions | Accepted | Timely useful | Checksum vs native |
| --- | ---: | ---: | ---: | --- |
| positive | 32444 | 16078 | 16078 | PASS |
| negative | 32444 | 16078 | 16078 | PASS |
| zero | 32446 | 16079 | 16079 | PASS |
| varying | 0 | 0 | 0 | PASS |
| independent | 0 | 0 | 0 | PASS |
| split | 30432 | 14068 | 14068 | PASS |
| forwarded | 16364 | 0 | 0 | PASS |

Every one of **144130** predictions matched the exact producer/dependent PCs,
`VA = pointer + signed delta`, and `tick >= ready`. Expected deltas were +8,
-8 and 0; varying and independent-overwrite controls produced zero predictions
and zero accepted prefetches. All observed prediction contexts were 0.
Every mode passed accepted/outcome conservation. Positive mode independently
recorded **14163 resident, 1965 deferred, 4 pending-MSHR subscriptions, 1962
returns**, including source-1 producer-prefetch returns. These are independently
observed values; they slightly differ from the author's counts, which are not
an exact acceptance threshold. The driver does not explain that variation.

Forwarded mode proves actual buffered completion, dependency training and
candidate generation, with **zero accepted/useful prefetches**. Split mode also
has buffered completion evidence and matching native values; its pointer array
starts at line offset 60 and includes page crossings. The runtime driver checks
buffered packets, not a per-load trace distinguishing every individual LSQ
forwarding/split mechanism. Ordinary demand hit/miss values use the same inspected
successful packet-completion path and the real guest exercises normal loads;
there is no separate assertion tagging one specific demand completion as a hit
and another as a miss.

Runtime config binds `cpu.p1_controller` to the actual L1D controller, source 2,
four observations and eight relation slots. P1 component target parameter -1 is
not its candidate destination: inspected `p1.cc` explicitly submits target 1;
positive runtime accounting has 16078 timely `P1_L1D` events and zero timely
P1 L2/L3 destination events. Lower-level P1 traffic exists as ordinary hierarchy
traffic. The shared inspected framework uses the saved request context and MMU
for virtual candidates. This establishes the exercised single-process context,
not multi-address-space correctness.

The executed `P1_DIRECTED_PASS` fixture invokes the real controller/P1/cache
code, with synthetic retired operations and fill injection where indicated. It
asserts: no recognition before the fourth distinct producer observation;
multiplier 1 to 2, repeated recognition remains 2, mismatch resets to 1 and
negative-offset retraining restores 2; two producer instances held in different
registers; overwrite rejection; separate P1 state namespace; no submission
before computation readiness; two exact consumers in one returned line;
security-domain mismatch rejection; fill and actual resident-cache readiness;
future split rejection; relation-slot epoch replacement; stream-version
replacement and old callback rejection; real I-cache invalidation cancellation.
It is component-state evidence, not a second real CPU application workload.

## Independent production regressions

All seven passed using the fixture-free production binary and unchanged frozen
parent infrastructure. For whole-program cases, native and guest stdout match;
classification is checked at the actual `visit` symbol. Crossline places that
instruction at page offset 4094; eviction requires multiple residency generations.
Every source's accepted count equals timely+late+unusedRemoved+aborted+unresolved.
Runtime config assertions confirm C-only ownership/source 3 and routes 1/2/3;
no-prefetch and naive have no I-cache instruction classification storage.

| Case | Profile | Accepted (active source) | Timely useful | Measured instructions |
| --- | --- | ---: | ---: | ---: |
| dense | tpc-c | 2696 | 2507 | 905773 |
| sparse | tpc-c | 0 | 0 | 501280 |
| crossline | tpc-c | 2703 | 2515 | 922064 |
| eviction | tpc-c | 7793 | 5710 | 1262005 |
| baseline | no-prefetch | 0 | 0 | 5000001 |
| prefetch | tpc-c | 43650 | 31993 | 5000000 |
| naive | naive-multi | 5860 | 4499 | 905773 |

Dense/sparse/crossline use `256 2 16`; eviction uses `dense 256 3 16 1`;
fixed-window baseline/C1 use `dense 8192 16 16`; naive uses `dense 256 2 16`.
The baseline and C1 each completed exactly 2000000 warmup instructions. Requested
measurement was 5000000; baseline measured 5000001 and C1 5000000, consistent
with the existing O3 commit-cycle overshoot contract. Statistics are extracted
from the measurement phase with cumulative instruction counters adjusted by the
unchanged runner. Windowed runs intentionally stop before guest exit, so no
whole-program checksum assertion is claimed for those two cases. No P1/T2
acceptances occurred in production baseline/C1/naive profiles. This is regression
evidence, not a paper speedup or incremental P1 performance result.

## Focused and host regressions on final source

- From parent cwd: `gem5/build/p1-venv/bin/python -m unittest discover -s benchmarks -p 'test_*.py'`:
  **20 passed**, `host-tests-final.log`. This reruns the prior preliminary 20
  tests in the correct cwd, without parent edits.
- `build/p1-venv/bin/python -m unittest discover -s tests/tpc -p 'test_pointer.py'`:
  **1 passed** (seven native modes and symbols).
- `g++ -std=c++17 -Isrc tests/tpc/p1_math.cc -o build/p1-independent-validation/p1-math-final`:
  binary **exit 0**, checked signed arithmetic/confidence, null, overflow,
  canonical/user bounds and page-crossing arithmetic.
- `g++ -std=c++17 -Isrc -Ibuild/X86 tests/tpc/state_tables.cc -o build/p1-independent-validation/state-tables-final`:
  binary **PASS / exit 0**, C1 state-table regressions.
- GCC16 lifetime-DSE reproduction: save original
  `git show 7e71b75f17173745826b368c7f4de8529b835e97:src/cpu/timebuf.hh`
  to `build/p1-independent-validation/original-headers/cpu/timebuf.hh`.
  Compile `tests/tpc/timebuf_init.cc` with
  `g++ -std=c++17 -O3 -DNDEBUG -Ibuild/p1-independent-validation/original-headers -Isrc`
  and separately with just `-Isrc`. **Original exit 1; fixed exit 0**.
  The focused test checks initial and recycled records over 32 advances.
  Inspection confirms placement `T()` initializes scalars inside their lifetime
  instead of relying on preconstruction memset. No gem5 rebuild was needed.

Focused results/commands are retained in `focused-runtime.log`; original/fixed
executables are `timebuf-before` / `timebuf-after`. Preliminary logs and binaries
are preserved separately. The allocator-cleanup failure in the old heap guest
was not rerun; bounded static guests passed here. General host-libc/SE heap
compatibility remains outside the validation claim.

## Final acceptance ledger and residual limits

| Expectation | Independent result and scope |
| --- | --- |
| Four consistent distinct instances; signed +8/-8/0 | PASS: directed threshold/mismatch and real guest traces |
| Unstable offsets / independent overwrite | PASS: zero candidates/acceptances in real controls; directed overwrite |
| Multiple producer instances | PASS: directed separate register tokens; broad mixed-token cases not separately executed |
| Once-only doubled lookahead | PASS: directed 1/2/2 and mismatch/retrain; real T2 algorithm absent |
| Resident / deferred / pending producer values | PASS: nonzero real counters/returns and directed exact line offsets |
| Readiness / security / multiple consumers | PASS: trace timestamps and directed resident/fill/security assertions |
| Demand hits/misses/forwarded/split | PASS for guest checksums, completion/training and buffered evidence; individual hit/miss attribution not separately instrumented |
| Source 2 / target 1 / context | PASS for actual config, L1D accounting, context 0 and inspected translating route; other contexts unverified |
| Stale source / stream / relation slot | PASS: executed producer-I-cache invalidation, stream replacement, old callback and slot-epoch tests |
| Dependent-only invalidation / timeout / queue bounds | Inspected guards; no dedicated executed directed case for each |
| Split/page behavior | PASS: demand split guest and host page arithmetic; future split rejection is intentional; no dedicated dependent cross-page translation/fault workload |
| C1 / no-prefetch / naive preservation | PASS: seven real production regressions plus host state tests |
| Trap/squash/reset, faulted loads, partial moves/XOR | Inspected integration/reset/dataflow paths; no dedicated runtime assertion for each |
| Full T2/TP/TPC and paper-suite performance | UNSUPPORTED: T2 absent; test-only producer is not TP/TPC validation |
| Linked-list self traversal | UNSUPPORTED / optional, outside promised arrays-of-pointers scope |
| Cache-port contention / Table II hardware area | Approximation; not modeled/validated |
| SMT, exec/JIT/address-space reuse, hot checkpoint serialization, vector/atomic/width variants | Outside documented supported scope; unverified |

Paper IV-B uses decode-time taint; this implementation uses committed
retirement-time register provenance and is later/conservative. Unsupported
memory taint, mixed instances and conditional/arithmetic over-taint mean this is
not exact bit-level slicing. Resident snapshots add readiness latency without
modeled cache-port contention. Future split subscriptions are rejected rather
than speculatively stitched. These deviations are explicitly documented in
`tests/tpc/docs/p1.md` and must accompany claims of paper fidelity.

The frozen system retains the paper's main Table I width/ROB/cache/clock/memory
sizes, but the paper's exact cache-port/latency/resource modeling and Table II
area equivalence are not established by these runs. No hardware-area equivalence,
paper-suite speedup, full TP/TPC applications or incremental P1 performance is
claimed. Original paper/proposal were read directly from the original workspace;
text extracts are build-local only.

No remaining blocker to publishing this scoped component validation was found.
At handback, parent status contained only unstaged `M gem5`; no parent commit
was created. Publication and any report push belong to the parent/implementation
owner; the validator created no PR and did not push.
