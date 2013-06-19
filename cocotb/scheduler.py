#!/usr/bin/env python

''' Copyright (c) 2013 Potential Ventures Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Potential Ventures Ltd nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL POTENTIAL VENTURES LTD BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. '''

"""
    Coroutine scheduler.
"""
import types
import threading
import collections
import os
import logging

import cocotb
import cocotb.decorators
from cocotb import triggers
from cocotb.handle import SimHandle
from cocotb.decorators import coroutine
from cocotb.triggers import Timer, ReadOnly, NextTimeStep, ReadWrite


class Scheduler(object):

    def __init__(self):
        self.waiting = collections.defaultdict(list)
        self.log = logging.getLogger("cocotb.scheduler")
        self.writes = {}
        self.writes_lock = threading.RLock()
        self._readwrite = self.add(self.move_to_rw())

    def react(self, trigger):
        """
        React called when a trigger fires.  We find any coroutines that are waiting on the particular
            trigger and schedule them.
        """
        trigger.log.debug("Fired!")

        if trigger not in self.waiting:
            # This isn't actually an error - often might do event.set() without knowing
            # whether any coroutines are actually waiting on this event
            # NB should catch a GPI trigger cause that would be catestrophic
            self.log.debug("Not waiting on triggger that fired! (%s)" % (str(trigger)))
            return

        # Scheduled coroutines may append to our waiting list
        # so the first thing to do is pop all entries waiting
        # on this trigger.
        self._scheduling = self.waiting.pop(trigger)
        to_run = len(self._scheduling)

        self.log.debug("%d pending coroutines for event %s" % (to_run, trigger))

        while self._scheduling:
            coroutine = self._scheduling.pop(0)
            self.schedule(coroutine, trigger=trigger)
            self.log.debug("Scheduled coroutine %s" % (coroutine.__name__))

        self.log.debug("Completed scheduling loop, still waiting on:")
        for trig, routines in self.waiting.items():
            self.log.debug("\t%s: [%s]" % (str(trig).ljust(30), " ".join([routine.__name__ for routine in routines])))

        # If we've performed any writes that are cached then schedule
        # another callback for the read-only part of the sim cycle
        if len(self.writes) and self._readwrite is None:
            self._readwrite = self.add(self.move_to_rw())
        return

    def playout_writes(self):
        if self.writes:
            if self._readwrite is None:
                self._readwrite = self.add(self.move_to_rw())
            while self.writes:
                handle, args = self.writes.popitem()
                handle.setimeadiatevalue(args)


    def save_write(self, handle, args):
        self.writes[handle]=args

    def _add_trigger(self, trigger, coroutine):
        """Adds a new trigger which will cause the coroutine to continue when fired"""
        self.waiting[trigger].append(coroutine)
        trigger.prime(self.react)


    def add(self, coroutine):
        """Add a new coroutine. Required because we cant send to a just started generator (FIXME)"""
        self.log.debug("Queuing new coroutine %s" % coroutine.__name__)
        self.schedule(coroutine)
        return coroutine


    def schedule(self, coroutine, trigger=None):
        coroutine.log.debug("Scheduling (%s)" % str(trigger))
        try:
            result = coroutine.send(trigger)
        except cocotb.decorators.CoroutineComplete as exc:
            self.log.debug("Coroutine completed execution with CoroutineComplete: %s" % coroutine.__name__)

            # Call any pending callbacks that were waiting for this coroutine to exit
            exc()
            return

        except cocotb.decorators.TestComplete as test:
            self.log.info("Test completed")
            # Unprime all pending triggers:
            for trigger, waiting in self.waiting.items():
                trigger.unprime()
                for coro in waiting:
                    try: coro.kill()
                    except StopIteration: pass
            self.waiting = []
            self.log.info("Test result: %s" % str(test.result))

            # FIXME: proper teardown
            stop

        if isinstance(result, triggers.Trigger):
            self._add_trigger(result, coroutine)
        elif isinstance(result, cocotb.decorators.coroutine):
            self.log.debug("Scheduling nested co-routine: %s" % result.__name__)
            # Queue this routine to schedule when the nested routine exits
            self._add_trigger(result.join(), coroutine)
            self.schedule(result)
        elif isinstance(result, list):
            for trigger in result:
                self._add_trigger(trigger, coroutine)
        else:
            self.log.warning("Unable to schedule coroutine since it's returning stuff %s" % repr(result))
        coroutine.log.debug("Finished sheduling coroutine (%s)" % str(trigger))

    @coroutine
    def move_to_rw(self):
        yield ReadWrite()
        self._readwrite = None
        self.playout_writes()
