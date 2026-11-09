# P1 validation record

This record distinguishes component evidence from full TP/TPC validation. Real
T2 is absent; no workload performance campaign or hydrated-checkpoint restore is
claimed.

Completed before full simulation:

- Prepared Python 3.11.18 with the project's locked dependency versions, now
  located at `build/p1-venv` inside gem5. System Python 3.14 was not used for
  the supported build. `tests/tpc/build-p1.sh` selects this environment's
  `python3-config` and shared-library path without changing the parent helper.
- All 20 unchanged parent host tests passed. The additional native guest test
  now lives in `tests/tpc/test_pointer.py` inside gem5. Run them separately with
  `(cd .. && gem5/build/p1-venv/bin/python -m unittest discover -s benchmarks -p 'test_*.py')`
  (one existing test is cwd-sensitive), and `build/p1-venv/bin/python -m unittest discover -s tests/tpc -p 'test_pointer.py'`.
- Existing C1 `state_tables.cc` passed with the production headers.
- `p1_math.cc` passed checked signed arithmetic, canonical/user address bounds,
  null/overflow, positive/negative/page-crossing offsets and confidence reset.
- Seven native static guests passed at 8192 entries × four rounds: positive,
  negative, zero, varying, independent, split and forwarded. The positive,
  independent, split and forwarded checksum was 402702336; negative 402636800;
  zero/varying 402669568.
- The built embedded-Python registration check passed: `P1Prefetcher` exists,
  `T2Prefetcher` does not, and all T/TP/TPC factories reject specifically the
  missing T2 component.
- Real SCons compilation succeeded for O3 commit/CPU/DynInst/LSQ, Classic
  BaseCache, framework, P1, producer fixture and directed fixture translation
  units. The modified x86 memory-op header passed a syntax check with
  `TRACING_ON=0`. This is compile evidence, not a simulation result.

Full gem5 build and runtime results are recorded below when available. Build
logs are retained locally at `/tmp/p1-focused-build.log` and `/tmp/p1-build.log`.
The reproducible runtime driver is `tests/tpc/validate_p1.py`; it checks actual
load symbols, native/guest checksums, predicted pointer-plus-offset addresses,
readiness timestamps and negative controls, and writes `build/p1-validation/report.json` with artifact hashes.

All paths in this record are relative to the gem5 repository unless absolute.
Only gem5 is to be committed; parent source edits authored during the initial
scope were restored after the user clarified gem5-only delivery.

## GCC 16 pipeline initialization blocker

The first linked binary crashed even with the no-prefetch profile, in O3
Decode/Rename/Commit with an uninitialized pipeline record count. Disassembly
showed that GCC 16 discarded the `memset` before placement construction in
`TimeBuffer`. `tests/tpc/timebuf_init.cc` reproduces the issue with the same
scalar-count plus intrusive-pointer-array shape: compiled at `-O3 -DNDEBUG`,
it exits 1 with the original header and 0 after the fix. The fix value-initializes
records inside their lifetime, both initially and when recycling a slot.

Reproduce from gem5:

```sh
g++ -std=c++17 -O3 -DNDEBUG -Isrc tests/tpc/timebuf_init.cc -o build/p1-validation/timebuf-init
build/p1-validation/timebuf-init
```

The original heap-backed guest also exposed a host static-libc/SE cleanup
failure (`double free or corruption (!prev)`, then libc `abort`/HLT) in the
zero-offset mode. It reproduced with **no prefetcher**, after the TimeBuffer
fix. The guest now uses bounded, aligned static arrays (up to 65536 elements)
so the P1 experiment does not depend on that allocator path. This does not
claim to fix general host-libc compatibility in gem5 SE. The diagnostic
no-prefetch output is retained in `build/p1-validation/baseline-zero.log`.

## Real simulator validation (2026-11-09)

Both builds linked successfully with Python 3.11.18, GCC 16,
`PROTOCOL=None USE_SYSTEMC=0`. The saved fixture binary is
`build/p1-validation/gem5-p1-fixture.fast` (SHA256
`c2637d8fd0f47e4ca17bce397927a525f5c34aaea49dc706e99d96ff2267b12b`).
The normal, fixture-free `build/X86/gem5.fast` SHA256 is
`3f0dfd4a6efc17dec9902025a72bff1da3e952a823970ea23d7307eb00792250`.
The final executable-source revision is `7ff5f09f63`; guest/driver revision is
`47aac84784`. Later documentation commits do not alter these binaries.
Production registration confirms P1 exists and both test fixtures and real T2
are absent. Build logs are under `build/p1-validation/build-*.log`.

The real directed fixture and all seven guest modes passed:

| Mode | P1 predictions | Accepted | Timely useful |
|---|---:|---:|---:|
| positive (+8) | 32446 | 16079 | 16079 |
| negative (-8) | 32444 | 16078 | 16078 |
| zero | 32446 | 16079 | 16079 |
| varying | 0 | 0 | 0 |
| independent overwrite | 0 | 0 | 0 |
| split demand | 30438 | 14071 | 14071 |
| store forwarded | 16367 | 0 | 0 |

Every guest checksum matched native execution. The driver checked actual load
PCs, every predicted pointer-plus-signed-offset address, submission no earlier
than modeled readiness, buffered completion for split/forwarded modes, and
accepted/outcome conservation. Forwarded mode proves completion/training and
candidate generation, **not** useful accepted prefetches: those demand-stage
candidates reach a line already being accessed.

The positive trace records 14165 resident subscriptions, 1963 deferred
subscriptions (four already in an MSHR), and 1960 delivered pointer values.
Source-1 producer-prefetch returns are present. These are real simulator paths
using an explicitly test-only producer, not only synthetic fill injections.
The full report with artifact hashes is `build/p1-validation/report.json`.
Reproduce using the saved fixture binary after building production:

```sh
build/p1-venv/bin/python tests/tpc/validate_p1.py \
  --gem5 build/p1-validation/gem5-p1-fixture.fast \
  --output build/p1-rerun
```

This is the implementation author's validation. Independent simulator validation
was handed to the parent thread and is pending separately; its owned report is
`docs/p1-independent-validation.md`. No full TP/TPC or workload speedup claim is
made while Member B's real T2 is absent.

The normal production binary also passed all seven original parent-worker
regression scenarios: C1 dense, sparse, cross-line and instruction eviction;
no-prefetch and C1 with 2M warmup/5M measurement; and naive-multi. Checks covered
checksums where measured to completion, original configuration attachments,
C1 classifications/generation changes, useful C1 activity and lifecycle
conservation for all sources. Parent runners/sources were unchanged. Local
artifacts: `build/p1-own-regressions/{run.log,regressions.json,regression-*/}`.
The copied orchestration script accepts both `t` and `T` from `nm` for `visit`
(the normal C guest's function is local); this is a test-script correction,
not an implementation or parent-source change.
