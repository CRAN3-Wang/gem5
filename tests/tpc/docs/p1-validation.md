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
