import random
import logging

import cocotb

from cocotb.clock import Clock
from cocotb.triggers import Timer, RisingEdge, ReadOnly
from cocotb.drivers import BitDriver
from cocotb.drivers.avalon import AvalonSTPkts as AvalonSTDriver
from cocotb.drivers.avalon import AvalonMaster
from cocotb.monitors.avalon import AvalonSTPkts as AvalonSTMonitor
from cocotb.regression import TestFactory
from cocotb.scoreboard import Scoreboard
from cocotb.result import TestFailure

# Data generators
from cocotb.generators.byte import random_data, get_bytes
from cocotb.generators.bit import wave, intermittent_single_cycles, random_50_percent

@cocotb.coroutine
def stream_out_config_setter(dut, stream_out, stream_in):
    """Coroutine to monitor the DUT configuration at the start
       of each packet transfer and set the endianness of the
       output stream accordingly"""
    edge = RisingEdge(dut.stream_in_startofpacket)
    ro = ReadOnly()
    while True:
        yield edge
        yield ro
        if dut.byteswapping.value:
            stream_out.config['firstSymbolInHighOrderBits'] = not \
             stream_in.config['firstSymbolInHighOrderBits']
        else:
            stream_out.config['firstSymbolInHighOrderBits'] = \
             stream_in.config['firstSymbolInHighOrderBits']

class EndianSwapperTB(object):

    def __init__(self, dut, debug=False):
        self.dut = dut
        self.stream_in  = AvalonSTDriver(dut, "stream_in", dut.clk)
        self.backpressure = BitDriver(self.dut.stream_out_ready, self.dut.clk)
        self.stream_out = AvalonSTMonitor(dut, "stream_out", dut.clk, 
                                  config={'firstSymbolInHighOrderBits': True})

        self.csr = AvalonMaster(dut, "csr", dut.clk)

        cocotb.fork(stream_out_config_setter(dut, self.stream_out, self.stream_in))

        # Create a scoreboard on the stream_out bus
        self.pkts_sent = 0
        self.expected_output = []
        self.scoreboard = Scoreboard(dut)
        self.scoreboard.add_interface(self.stream_out, self.expected_output)

        # Reconstruct the input transactions from the pins and send them to our 'model'
        self.stream_in_recovered = AvalonSTMonitor(dut, "stream_in", dut.clk, callback=self.model)

        # Set verbosity on our various interfaces
        level = logging.DEBUG if debug else logging.WARNING
        self.stream_in.log.setLevel(level)
        self.stream_in_recovered.log.setLevel(level)

    def model(self, transaction):
        """Model the DUT based on the input transaction"""
        self.expected_output.append(transaction)
        self.pkts_sent += 1

    @cocotb.coroutine
    def reset(self, duration=10000):
        self.dut.log.debug("Resetting DUT")
        self.dut.reset_n <= 0
        self.stream_in.bus.valid <= 0
        yield Timer(duration)
        yield RisingEdge(self.dut.clk)
        self.dut.reset_n <= 1
        self.dut.log.debug("Out of reset")


@cocotb.coroutine
def clock_gen(signal):
    while True:
        signal <= 0
        yield Timer(5000)
        signal <= 1
        yield Timer(5000)

@cocotb.coroutine
def run_test(dut, data_in=None, config_coroutine=None, idle_inserter=None, backpressure_inserter=None):

    cocotb.fork(clock_gen(dut.clk))
    tb = EndianSwapperTB(dut)
    
    yield tb.reset()
    dut.stream_out_ready <= 1

    # Start off any optional coroutines
    if config_coroutine is not None:
        cocotb.fork(config_coroutine(tb.csr))
    if idle_inserter is not None:
        tb.stream_in.set_valid_generator(idle_inserter())
    if backpressure_inserter is not None:
        tb.backpressure.start(backpressure_inserter())

    # Send in the packets
    for transaction in data_in():
        yield tb.stream_in.send(transaction)
       
    # Wait at least 2 cycles where output ready is low before ending the test
    for i in xrange(2):
        yield RisingEdge(dut.clk)
        while not dut.stream_out_ready.value:
            yield RisingEdge(dut.clk)
 
    pkt_count = yield tb.csr.read(1)

    if pkt_count.integer != tb.pkts_sent:
        raise TestFailure("DUT recorded %d packets but tb counted %d" % (
                          pkt_count.integer, tb.pkts_sent))
    else:
        dut.log.info("DUT correctly counted %d packets" % pkt_count.integer)

    raise tb.scoreboard.result


def random_packet_sizes(min_size=1, max_size=150, npackets=10):
    """random string data of a random length"""
    for i in xrange(npackets):
        yield get_bytes(random.randint(min_size, max_size), random_data())

@cocotb.coroutine
def randomly_switch_config(csr):
    """Twiddle the byteswapping config register"""
    while True:
        yield csr.write(0, random.randint(0,1))


factory = TestFactory(run_test)
factory.add_option("data_in",                 [random_packet_sizes])
factory.add_option("config_coroutine",        [None, randomly_switch_config])
factory.add_option("idle_inserter",           [None, wave, intermittent_single_cycles, random_50_percent])
factory.add_option("backpressure_inserter",   [None, wave, intermittent_single_cycles, random_50_percent])
factory.generate_tests()

