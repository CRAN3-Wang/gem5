"""Run after TPC_TESTS=1 JOBS=2 ./build-gem5-fast.sh. No guest execution."""
import argparse
from pathlib import Path
import sys
import m5
from m5.objects import Root, TPCStateFixture, TPCFixtureComponent, FrameworkQueue, DivisionOfLaborController
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'configs'))
import prefetchers
from system import make_system
parser = argparse.ArgumentParser()
parser.add_argument('--policy', action='store_true')
parser.add_argument('--binary', type=Path, default=ROOT/'results/tpc-test-program')
parser.add_argument('--owner', type=int, choices=(1, 2, 3),
                    help='Run a guest with colliding recognizers at and below this priority')
args = parser.parse_args()
policy = args.policy or args.owner is not None
if policy:
    def factory():
        routes = [FrameworkQueue(accounting_target=k) for k in (1,2,3)]
        # Deliberately reverse order; T recognizes but generates no candidates.
        components = [TPCFixtureComponent(accounting_source=k, recognize=k >= (args.owner or 1)) for k in (3,2,1)]
        layers = prefetchers.no_prefetch()
        layers.update(l1d=DivisionOfLaborController(mode='TPC',routes=routes,components=components),
                      l2=routes[1],l3=routes[2])
        return layers
    prefetchers.PROFILES['fixture'] = factory
folder=Path(m5.options.outdir).resolve()
spec=dict(prefetcher='fixture' if policy else 'tpc-c', binary=str(args.binary.resolve()),
          args=['positive', '128', '1'] if args.owner else [],cwd=str(folder),outdir=str(folder))
system=make_system(spec)
controller=system.cpu.dcache.prefetcher
component=next(c for c in controller.components if int(c.accounting_source)==(1 if policy else 3))
if args.owner is None:
    system.fixture=TPCStateFixture(cache=system.cpu.icache,controller=controller,component=component)
root=Root(full_system=False,system=system)
m5.instantiate()
# LRU uses tick 0 for invalid entries. Avoid an artificial all-zero tie.
if args.owner is None:
    m5.simulate(1)
    system.fixture.run()
    print('TPC_LIFECYCLE_PASS', flush=True)
else:
    event = m5.simulate(100_000_000_000)
    if event.getCause() != 'exiting with last active thread context' or event.getCode():
        raise RuntimeError((event.getCause(), event.getCode()))
    print('TPC_POLICY_GUEST_PASS', args.owner, flush=True)
