"""Production P1/O3 with an explicitly test-only producer (real T2 is absent).
Build with TPC_TESTS=1. Arguments: binary producer_pc [mode] [--directed].
"""
from pathlib import Path
import sys
import m5
from m5.objects import (Root, FrameworkQueue, DivisionOfLaborController,
                        P1Prefetcher, P1ProducerFixture, TPCStateFixture)
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "configs"))
import prefetchers
from system import make_system
binary = str(Path(sys.argv[1]).resolve())
producer = int(sys.argv[2], 0)
mode = sys.argv[3] if len(sys.argv) > 3 and not sys.argv[3].startswith("--") else "positive"
def fixture():
    routes = [FrameworkQueue(accounting_target=k, cache_snoop=True,
                            throttle_control_percentage=0) for k in (1, 2, 3)]
    components = [P1ProducerFixture(producer_pc=producer), P1Prefetcher(trace=True)]
    layers = prefetchers.no_prefetch()
    layers.update(l1d=DivisionOfLaborController(mode="TP", routes=routes, components=components),
                  l2=routes[1], l3=routes[2])
    return layers
prefetchers.PROFILES["p1-fixture"] = fixture
folder = Path(m5.options.outdir).resolve()
system = make_system(dict(prefetcher="p1-fixture", binary=binary,
                         args=[mode, "4096", "4"], cwd=str(folder), outdir=str(folder)))
assert system.cpu.p1_controller is system.cpu.dcache.prefetcher, "P1 retirement binding missing"
if "--directed" in sys.argv:
    controller = system.cpu.dcache.prefetcher
    system.fixture = TPCStateFixture(cache=system.cpu.icache, data_cache=system.cpu.dcache,
                                    controller=controller,
                                    component=controller.components[1])
root = Root(full_system=False, system=system)
m5.instantiate()
if "--directed" in sys.argv:
    m5.simulate(1)
    system.fixture.run()
else:
    event = m5.simulate(100_000_000_000)
    if event.getCause() != "exiting with last active thread context" or event.getCode():
        raise RuntimeError(f"guest failed: {event.getCause()}, code={event.getCode()}")
m5.stats.dump()
print("P1_FIXTURE_PASS")
