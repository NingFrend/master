# -*- coding: utf-8 eval: (blacken-mode 1) -*-
# SPDX-License-Identifier: ISC
#
# January 23 2024, Christian Hopps <chopps@labn.net>
#
# Copyright (c) 2024, LabN Consulting, L.L.C.
#

"""
Test YANG Notifications
"""
import json
import logging
import os

import pytest
from lib.topogen import Topogen
from lib.topotest import json_cmp
from oper import check_kernel_32

pytestmark = [pytest.mark.ripd, pytest.mark.staticd, pytest.mark.mgmtd]

CWD = os.path.dirname(os.path.realpath(__file__))


@pytest.fixture(scope="module")
def tgen(request):
    "Setup/Teardown the environment and provide tgen argument to tests"

    topodef = {
        "s1": ("r1", "r2"),
    }

    tgen = Topogen(topodef, request.module.__name__)
    tgen.start_topology()

    router_list = tgen.routers()
    for rname, router in router_list.items():
        router.load_frr_config("frr.conf")

    tgen.start_router()
    yield tgen
    tgen.stop_topology()


def test_oper_simple(tgen):
    if tgen.routers_have_failure():
        pytest.skip(tgen.errors)

    r1 = tgen.gears["r1"].net
    check_kernel_32(r1, "11.11.11.11", 1, "")

    fe_client_path = CWD + "/../lib/fe_client.py"
    output = r1.cmd_raises(fe_client_path + " --listen")
    jsout = json.loads(output)

    expected = {"frr-ripd:authentication-type-failure": {"interface-name": "r1-eth0"}}
    result = json_cmp(jsout, expected)
    assert result is None
