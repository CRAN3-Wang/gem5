"""Optional directed test objects; build with TPC_TESTS=1."""
from m5.SimObject import SimObject, PyBindMethod
from m5.params import *
from m5.objects.Framework import TPCComponent
class TPCStateFixture(SimObject):
    type = "TPCStateFixture"
    cxx_class = "gem5::prefetch::StateFixture"
    cxx_header = "mem/cache/prefetch/state_fixture.hh"
    cxx_exports = [PyBindMethod("run")]
    cache = Param.BaseCache("I-cache under test")
    data_cache = Param.BaseCache(NULL, "Optional P1 resident-value cache")
    controller = Param.TPCController("Bound controller")
    component = Param.TPCComponent("Component under test")
class TPCFixtureComponent(TPCComponent):
    type = "TPCFixtureComponent"
    cxx_class = "gem5::prefetch::FixtureComponent"
    cxx_header = "mem/cache/prefetch/state_fixture.hh"
    recognize = Param.Bool(True, "Recognize without generating candidates")

class P1ProducerFixture(TPCComponent):
    type = "P1ProducerFixture"
    cxx_class = "gem5::prefetch::P1ProducerFixture"
    cxx_header = "mem/cache/prefetch/p1_fixture.hh"
    accounting_source = 1
    producer_pc = Param.Addr("Exact fixture guest producer load PC")
