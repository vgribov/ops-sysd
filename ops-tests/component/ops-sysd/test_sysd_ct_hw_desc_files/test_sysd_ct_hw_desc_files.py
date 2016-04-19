# (C) Copyright 2016 Hewlett Packard Enterprise Development LP
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
OpenSwitch Test for switchd related configurations.
"""

from pytest import mark
from time import sleep
import yaml
import shutil
import os.path

TOPOLOGY = """
# +-------+
# |  ops1 |
# +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
"""


ovs_vsctl = "/usr/bin/ovs-vsctl "
ovs_appctl = "/usr/bin/ovs-appctl "
ovsdb_tool = "/usr/bin/ovsdb-tool "


def start(dut):
    start_ovsdb(dut)
    sleep(3)
    start_sysd(dut)
    wait_until_ovsdb_is_up(dut)


def stop(dut):
    stop_sysd(dut)
    stop_ovsdb(dut)
    sleep(3)


def start_sysd(dut):
    dut("/bin/systemctl start ops-sysd", shell="bash")


def stop_sysd(dut):
    dut(ovs_appctl + "-t ops-sysd exit", shell="bash")


def start_ovsdb(dut):
    """Create an empty DB file and load it into ovsdb-server."""

    # Create an empty database file.
    c = (
        "{ovsdb_tool} create /var/run/openvswitch/ovsdb.db "
        "/usr/share/openvswitch/vswitch.ovsschema".format(
            ovsdb_tool=ovsdb_tool
        )
    )
    dut(c, shell="bash")

    # Load the newly created DB into ovsdb-server
    dut(ovs_appctl + "-t ovsdb-server ovsdb-server/add-db "
        "/var/run/openvswitch/ovsdb.db", shell="bash")


def stop_ovsdb(dut):
    """Remove the OpenSwitch DB from ovsdb-server.

    It also removes the DB file from the file system.
    """

    # Remove the database from the ovsdb-server.
    dut(ovs_appctl + "-t ovsdb-server ovsdb-server/remove-db "
        "OpenSwitch", shell="bash")

    # Remove the DB file from the file system.
    dut("/bin/rm -f /var/run/openvswitch/ovsdb.db", shell="bash")


def copy_hw_desc_files(dut):
    """Copy Hardware description files to /etc/openswitch/hwdesc"""
    c = "cp -a /tmp/hwdesc/*.yaml etc/openswitch/hwdesc"
    dut(c, shell="bash")


def read_yaml_port_file(file_name):
    """Read the local port.yaml file and return the data."""
    cur_dir, f = os.path.split(__file__)

    yaml_file_loc = cur_dir + "/hwdesc/" + file_name

    yaml_data = open(yaml_file_loc, "r")
    return yaml.load(yaml_data)


def get_subsystem_other_info(dut, map_key):
    """Get the value from subsystem table other_info column."""
    subsystem_uuid = dut(ovs_vsctl + "list subsystem | grep -i uuid|cut -d :"
                         " -f 2", shell="bash")
    uuid = subsystem_uuid.replace('\r\n', '')
    c = ovs_vsctl + "get subsystem " + uuid + " other_info:" + map_key
    out = dut(c, shell="bash")
    return out.replace('\r\n', '')


def get_interface_hw_info(dut, name, map_key):
    """Get the value from interface table hw_intf_info column."""
    c = ovs_vsctl + "get interface " + str(name) + \
        " hw_intf_info:" + map_key
    out = dut(c, shell="bash")
    return out.replace('\r\n', '')


def wait_until_ovsdb_is_up(dut):
    """Wait until System table is visible in the ovsdb-server."""
    cmd = ovs_vsctl + "list System | grep uuid"
    wait_count = 20
    while wait_count > 0:
        out = dut(cmd, shell="bash")
        if "_uuid" in out:
            break

        wait_count -= 1
        sleep(1)
    assert wait_count != 0


@mark.skipif(True, reason="OVSDB error when trying to bring up sysd process")
def test_sysd_ct_hw_desc_files(topology, step):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    cur_dir, f = os.path.split(__file__)
    test_file_dir = os.path.join(cur_dir, "hwdesc")

    shutil.copytree(test_file_dir, ops1.shared_dir + "/hwdesc")

    stop(ops1)
    copy_hw_desc_files(ops1)
    start(ops1)
    expected = read_yaml_port_file("ports.yaml")

    x, count = get_subsystem_other_info(ops1, "interface_count").splitlines()
    exp = '"{out}"'.format(
        out=expected['port_info']['number_ports']
    )
    assert count == exp

    x, count = get_subsystem_other_info(ops1, "max_bond_count").splitlines()
    exp = str(expected['port_info']['max_lag_count'])
    assert count == '"{out}"'.format(out=exp)

    x, count = get_subsystem_other_info(ops1,
                                        "max_bond_member_count").splitlines()
    exp = str(expected['port_info']['max_lag_member_count'])
    assert count == '"{out}"'.format(out=exp)

    port_name = expected['ports'][0]['name']
    intf_id = get_interface_hw_info(ops1, port_name,
                                    "switch_intf_id")
    exp = str(expected['ports'][0]['switch_device_port'])
    assert intf_id == '"{out}"'.format(out=exp)

    port_name = expected['ports'][0]['name']
    connector_type = get_interface_hw_info(ops1, port_name,
                                           "connector")
    exp = str(expected['ports'][0]['connector'])
    assert connector_type == '"{out}"'.format(out=exp)

    name = ops1(ovs_vsctl + "get bridge bridge_normal name",
                shell="bash")
    name = name.replace('\r\n', '')
    exp = "bridge_normal"
    assert name == exp
