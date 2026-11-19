"""Focused T2 functional run using the frozen team topology (no checkpoint)."""
import argparse
from pathlib import Path
import sys
import m5
from m5.objects import Root
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'configs'))
from system import make_system
parser=argparse.ArgumentParser()
parser.add_argument('--binary',required=True)
parser.add_argument('--profile',choices=('tpc-t','tpc-tp','tpc','tpc-c','no-prefetch'),default='tpc-t')
parser.add_argument('--mapped-pages',action='store_true',
                    help='Map the pointer_edges guest array to nonadjacent physical pages')
parser.add_argument('--production',action='store_true',help='Require a fixture-free build')
parser.add_argument('--pressure',action='store_true',
                    help='Exercise a single pending P1 value slot')
parser.add_argument('--trace',action='store_true')
parser.add_argument('--max-ticks',type=int,default=2_000_000_000)
parser.add_argument('--latency-mode',choices=('service','demand'),default='service')
parser.add_argument('--margin-cycles',type=int,default=None,
                    help='Override the team profile margin; omitted preserves its tuned value')
parser.add_argument('--dram-timing-scale',type=float,default=1,
                    help='Synthetic DRAM timing sensitivity; 1 preserves the frozen model')
parser.add_argument('--args',nargs='*',default=[])
a=parser.parse_args()
if a.production and hasattr(m5.objects, 'TPCStateFixture'):
    parser.error('TPC acceptance requires a production fast build')
if (a.margin_cycles is not None and a.margin_cycles < 0) or not 0 < a.dram_timing_scale <= 4 or a.max_ticks <= 0:
    parser.error('margin must be nonnegative; DRAM timing scale must be in (0,4]; max ticks must be positive')
folder=Path(m5.options.outdir).resolve()
spec=dict(prefetcher=a.profile,binary=str(Path(a.binary).resolve()),args=a.args,
          cwd=str(folder),outdir=str(folder))
system=make_system(spec)
controller=system.cpu.dcache.prefetcher
if a.profile in ('tpc-t','tpc-tp','tpc'):
    assert system.cpu.tpc_adapter.controller is controller
    assert system.cpu.p1_controller is controller
    for component in controller.components:
        component.trace=a.trace
        if a.pressure and int(component.accounting_source) == 2:
            component.pending_values=1
        if int(component.accounting_source) == 1:
            if a.margin_cycles is not None:
                component.margin_cycles=a.margin_cycles
            component.latency_mode=a.latency_mode
if a.dram_timing_scale != 1:
    from m5.params import Latency
    # Synthetic sensitivity only: scale concrete latency values, preserving proxies.
    # This also scales DRAM clock/burst/refresh timing, not the caches or buses.
    for mem in system.mem_ctrls:
        for name in mem.dram._params.keys():
            value=getattr(mem.dram,name)
            if isinstance(value,Latency):
                setattr(mem.dram,name,str(float(value)*a.dram_timing_scale)+'s')
root=Root(full_system=False,system=system)
m5.instantiate()
if a.mapped_pages:
    # Dedicated SE guest uses eight pages; high physical frames are outside
    # this small process's normal allocations, inside the frozen 4 GiB RAM.
    for page in range(8):
        system.cpu.workload[0].map(0x200000000 + 4096 * page,
                                   0x70000000 + 8192 * page, 4096, True)
event=m5.simulate(a.max_ticks)
if event.getCause()!='exiting with last active thread context' or event.getCode()!=0:
    raise RuntimeError((event.getCause(),event.getCode()))
print('TPC_EXIT', event.getCause(), 'code', event.getCode())
print('TPC_FUNCTIONAL_PASS',a.profile,'tick',m5.curTick(),flush=True)
