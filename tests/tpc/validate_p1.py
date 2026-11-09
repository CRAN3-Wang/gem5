#!/usr/bin/env python3
"""Run opt-in production-P1 fixtures; never a full T2/TP performance evaluation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def run(command, log, env):
    with log.open("w") as output:
        subprocess.run(list(map(str, command)), cwd=ROOT, env=env, stdout=output,
                       stderr=subprocess.STDOUT, check=True, timeout=900)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gem5", type=Path, default=ROOT / "build/X86/gem5.fast")
    parser.add_argument("--output", type=Path, default=ROOT / "build/p1-validation")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = str(Path(sys.base_prefix) / "lib") + ":" + env.get("LD_LIBRARY_PATH", "")
    binary = out / "pointer"
    run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-fno-pie", "-no-pie",
         ROOT / "tests/tpc/pointer.c", "-o", binary], out / "compile.log", env)
    symbols = subprocess.check_output(["nm", str(binary)], text=True)
    producer = int(re.search(r"^([0-9a-f]+) T p1_producer$", symbols, re.M)[1], 16)
    dependent = int(re.search(r"^([0-9a-f]+) T p1_dependent$", symbols, re.M)[1], 16)
    config = ROOT / "tests/tpc/p1.py"
    run([args.gem5, "-d", out / "directed", config, binary, "0x400007", "--directed"],
        out / "directed.log", env)
    assert "P1_DIRECTED_PASS" in (out / "directed.log").read_text()
    results = {}
    for mode in ("positive", "negative", "zero", "varying", "independent", "split", "forwarded"):
        run([binary, mode, "4096", "4"], out / f"{mode}.native", env)
        folder = out / mode
        log = out / f"{mode}.log"
        run([args.gem5, "-d", folder, config, binary, hex(producer), mode], log, env)
        assert (folder / "benchmark.stdout").read_text() == (out / f"{mode}.native").read_text(), mode
        text = log.read_text()
        assert "P1_FIXTURE_PASS" in text, mode
        predictions = []
        for line in text.splitlines():
            if "P1_PREDICT" not in line:
                continue
            values = {key: int(value, 0) for key, value in re.findall(r"(\w+)=(-?0x[0-9a-f]+|-?\d+)", line)}
            assert values["producer"] == producer and values["dependent"] == dependent, line
            assert values["va"] == values["pointer"] + values["delta"], line
            assert values["tick"] >= values["ready"], line
            predictions.append(values)
        if mode in ("varying", "independent"):
            assert not predictions, mode
        else:
            assert predictions, f"no P1 predictions for {mode}"
            expected = -8 if mode == "negative" else 0 if mode == "zero" else 8
            assert all(p["delta"] == expected for p in predictions), mode
        if mode in ("split", "forwarded"):
            assert re.search(r"P1_VALUE .*buffered=1", text), f"no buffered producer completion: {mode}"
        stats = {}
        for line in (folder / "stats.txt").read_text().splitlines():
            fields = line.split()
            if len(fields) > 1:
                try:
                    stats[fields[0]] = float(fields[1])
                except ValueError:
                    pass
        value = lambda metric: stats["system.prefetch_accounting." + metric + "::P1"]
        outcomes = ("timelyUseful", "late", "unusedRemoved", "aborted", "unresolved")
        assert value("accepted") == sum(value(k) for k in outcomes), mode
        paths = {name: stats["system.cpu.dcache.prefetcher.components1.p1." + name]
                 for name in ("residentValues", "deferredValues", "pendingValues", "returns")}
        if mode == "positive":
            assert all(paths.values()), paths
            assert "origin_source=1" in text
            assert value("accepted") > 0 and value("timelyUseful") > 0
        if mode in ("varying", "independent"):
            assert value("accepted") == 0
        results[mode] = {"checksum_matches": True, "predictions": len(predictions),
                         "return_paths": paths, "accepted": value("accepted"),
                         "outcomes": {k: value(k) for k in outcomes}}
    identities = {}
    for path in (args.gem5, binary, ROOT / "tests/tpc/pointer.c"):
        identities[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    report = {"sha256": identities, "kind": "P1 with test-only producer; real T2 absent", "producer_pc": hex(producer),
              "dependent_pc": hex(dependent), "directed": "pass", "modes": results}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
