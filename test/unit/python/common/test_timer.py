"""Unit tests for timing facilities"""

# Copyright (C) 2017 Jan Blechta
#
# This file is part of DOLFIN.
#
# DOLFIN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# DOLFIN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.

import pytest

import gc
import uuid
from time import sleep

from dolfin import *


def get_random_task_name():
    return uuid.uuid4().hex


def test_context_manager_named():
    task = get_random_task_name()
    with Timer(task) as t:
        sleep(0.05)
        assert t.elapsed()[0] >= 0.05
    t = timing(task, TimingClear_clear)
    assert t[0] == 1
    assert t[1] >= 0.05


def test_context_manager_anonymous():
    with Timer() as t:
        sleep(0.05)
        assert t.elapsed()[0] >= 0.05


# Test case for free function
fun2_task = get_random_task_name()
@timed(fun2_task)
def fun2(*args, **kwargs):
    "Foo"
    sleep(0.05)
    return args, kwargs

class C(object):
    # Test case for instancemethod
    task_method2 = get_random_task_name()
    @timed(task_method2)
    def method2(self, *args, **kwargs):
        "Foo"
        sleep(0.05)
        return args, kwargs

    # Test case for staticmethod
    task_method3 = get_random_task_name()
    @staticmethod
    @timed(task_method3)
    def method3(*args, **kwargs):
        "Foo"
        sleep(0.05)
        return args, kwargs

    # Test case for classmethod
    task_method5 = get_random_task_name()
    @classmethod
    @timed(task_method5)
    def method5(cls, *args, **kwargs):
        "Foo"
        sleep(0.05)
        return args, kwargs

o = C()

@pytest.mark.parametrize(("fun", "task"), [
    (fun2, fun2_task),
    (o.method2, o.task_method2),
    (o.method3, o.task_method3),
    (o.method5, o.task_method5),
    (C.method3, C.task_method3),
    (C.method5, C.task_method5),
])
def test_decorator_functionality(fun, task):
    assert fun.__doc__ == "Foo"
    assert fun(1, 2, 3, four=5) == ((1, 2, 3), {'four': 5})
    assert fun(1, 2, 3, four=5) == ((1, 2, 3), {'four': 5})
    t = timing(task, TimingClear_clear)
    assert t[0] == 2
    assert t[1] >= 0.1


def test_decorator_timer_scope():
    """Check that Timer object is not launched by decorator
    unless function is invoked and logging the timing when
    destroyed without usage"""
    task = get_random_task_name()

    # Delete eventual previous timing
    try:
        timing(task, TimingClear_clear)
    except RuntimeError:
        pass

    @timed(task)
    def foo():
        pass

    # Delete function (including eventual timer which should not
    # exist yet) before using it
    del foo
    gc.collect()

    # Check that there's no timing entry
    with pytest.raises(RuntimeError):
        timing(task, TimingClear_clear)


def test_decorator_timing_correctness():
    task = get_random_task_name()

    @timed(task)
    def foo():
        pass

    sleep(0.05)
    foo()
    sleep(0.05)

    # Check that sleeping above did not influence timer
    assert timing(task, TimingClear_clear)[1] < 0.1
