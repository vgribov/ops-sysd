# -*- coding: utf-8 -*-
# (C) Copyright 2015 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
##########################################################################

"""
OpenSwitch Test for checking platform daemons and for errors while bootup.
"""

from __future__ import unicode_literals, absolute_import
from __future__ import print_function, division

from time import sleep

from platform_err_msgs import err_msgs

TOPOLOGY = """
# +-----+   +------+   +-------+
# | hs1 +---+ ops1 +---+  hs2  |
# +-----+   +------+   +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

hs1:1 -- ops1:1
hs2:1 -- ops1:2
"""

ops_intf_id1 = "10.0.10.2"
ops_intf_id2 = "10.0.20.2"

network_pl = "24"
ops_intf_ids = [ops_intf_id1, ops_intf_id2]

dutarray = []
hstarray = []

hw_daemons = [
    'ops-switchd',
    'ops-sysd',
    'ops-pmd',
    'ops-tempd',
    'ops-powerd',
    'ops-ledd',
    'ops-fand']

msgs = err_msgs()


def wait_until_interface_up(switch, portlbl, timeout=30, polling_frequency=1):
    """
    Wait until the interface, as mapped by the given portlbl, is marked as up.

    :param switch: The switch node.
    :param str portlbl: Port label that is mapped to the interfaces.
    :param int timeout: Number of seconds to wait.
    :param int polling_frequency: Frequency of the polling.
    :return: None if interface is brought-up. If not, an assertion is raised.
    """
    for i in range(timeout):
        status = switch.libs.vtysh.show_interface(portlbl)
        if status['interface_state'] == 'up':
            break
        sleep(polling_frequency)
    else:
        assert False, (
            'Interface {}:{} never brought-up after '
            'waiting for {} seconds'.format(
                switch.identifier, portlbl, timeout
            )
        )


def configure_ips(step):
    step("Configuring switch IPs...")

    sw1 = dutarray[0]
    hs1 = hstarray[0]
    hs2 = hstarray[1]

    # Configure host interfaces
    step("### Configuring host interfaces ###")
    hs1.libs.ip.interface('1', addr='10.0.10.1/24', up=True)
    hs2.libs.ip.interface('1', addr='10.0.20.3/24', up=True)

    # Add routes on hosts
    step("### Adding routes on hosts ###")
    hs1.libs.ip.add_route('10.0.20.0/24', '10.0.10.2')
    hs2.libs.ip.add_route('10.0.10.0/24', '10.0.20.2')

    # Configure IP and bring UP switch 1 interfaces
    with sw1.libs.vtysh.ConfigInterface('1') as ctx:
        ip_addr = ops_intf_ids[0] + "/" + network_pl
        ctx.ip_address(ip_addr)
        ctx.no_shutdown()

    with sw1.libs.vtysh.ConfigInterface('2') as ctx:
        ip_addr = ops_intf_ids[1] + "/" + network_pl
        ctx.ip_address(ip_addr)
        ctx.no_shutdown()


def verify_bootup(step):
    step("Verifying platform processes running...")

    switch = dutarray[0]

    dump = switch("ps -e", shell="bash")

    for daemon in hw_daemons:
        if daemon in str(dump):
            step("%s daemon is running" % (daemon))
        else:
            assert False, step("%s daemon is not running" % (daemon))


def verify_bootup_logs(step):
    step("Finding errors while system bootup...")

    switch = dutarray[0]

    switch("cp /var/log/messages /messages", shell="bash")

    for err in msgs:
        sh_cmd = 'cat /messages | grep "' + err + '"'
        cmd_out = switch(sh_cmd, shell="bash")
        lst = cmd_out.split('\n')
        # lst.pop(0)
        for item in lst:
            if err in item:
                assert False, step("There is an error while system bootup")


def verify_ping(step):
    step("Verifying IPv4 ping...")

    sw1 = dutarray[0]
    hs1 = hstarray[0]

    # Wait until interfaces are up
    for switch, portlbl in [(sw1, '1'), (sw1, '2')]:
        wait_until_interface_up(switch, portlbl)

    sleep(2)

    # Ping IPv4-address from host1 to host2
    step("Test ping IPv4-address from hs1 to hs2")
    ping = hs1.libs.ping.ping(1, '10.0.20.3')

    assert ping['transmitted'] == ping['received'] == 1, "Ping"
    " between hosts failed"


def test_ft_bootup(topology, step):
    ops1 = topology.get("ops1")
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    global dutarray
    dutarray = [ops1]

    global hstarray
    hstarray = [hs1, hs2]

    configure_ips(step)

    verify_bootup(step)
    verify_bootup_logs(step)
    verify_ping(step)
