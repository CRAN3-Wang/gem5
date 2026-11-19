"""Directed production T2/P1 connector checks; requires TPC_TESTS=1.

Usage: gem5.fast tests/tpc/connector.py /path/to/any/x86/guest
The guest is instantiated but never executed.
"""
from pathlib import Path
import sys

import m5
from m5.objects import Root, TPCStateFixture

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "configs"))
from system import make_system

folder = Path(m5.options.outdir).resolve()
system = make_system(dict(prefetcher="tpc-tp", binary=str(Path(sys.argv[1]).resolve()),
                          args=[], cwd=str(folder), outdir=str(folder)))
controller = system.cpu.dcache.prefetcher
components = {int(c.accounting_source): c for c in controller.components}
system.fixture = TPCStateFixture(cache=system.cpu.icache, data_cache=system.cpu.dcache,
                                 controller=controller, component=components[2],
                                 producer=components[1])
root = Root(full_system=False, system=system)
m5.instantiate()
m5.simulate(1)
system.fixture.run()
print("TPC_CONNECTOR_PASS", flush=True)
