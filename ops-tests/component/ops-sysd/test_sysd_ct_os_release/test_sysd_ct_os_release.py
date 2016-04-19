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
OpenSwitch Test for switchd related configurations.
"""

# from pytest import set_trace
from time import sleep
import json
import shutil
import os.path
import pytest

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

default_os_release_file = "os-release.default"
os_release_files_dir = "files/os_releases"


def check_os_name(dut, file_name):
    """
    Testing ops-sysd correctly stores switch_info:os_name value.

    Test if the ops-sysd correctly parse the os-release file and stores
    the information in the OVSDB.
    """
    copy_os_release_file(dut, file_name)
    # Restart the ovsdb-server and sysd
    start(dut)

    expected = read_os_release_file(dut, file_name, key='NAME')
    result = get_software_info(dut, key='os_name')

    assert result == expected


def check_switch_version(dut, file_name):
    """
    Testing ops-sysd correctly stores switch_version column.

    Test if the ops-sysd correctly parse the os-release file and stores
    the information in the OVSDB.
    """
    copy_os_release_file(dut, file_name)
    # Restart the ovsdb-server and sysd
    start(dut)

    version_id = read_os_release_file(dut, file_name, 'VERSION_ID')
    build_id = read_os_release_file(dut, file_name, 'BUILD_ID')
    expected = "{0} (Build: {1})".format(version_id, build_id)
    result = get_switch_version(dut)

    assert result == expected


def get_software_info(dut, key=None):
    out = dut(ovs_vsctl + "--format json list system", shell="bash")
    data = json.loads(out)['data']
    i = get_system_column_id(dut, 'software_info')
    print(data[0][i])
    if key:
        for (k, v) in data[0][i][1]:
            if k == key:
                return v
    else:
        return data[0][i]


def get_switch_version(dut):
    out = dut(ovs_vsctl + "--format json list system", shell="bash")
    data = json.loads(out)['data']
    i = get_system_column_id(dut, 'switch_version')
    return data[0][i]


def get_system_column_id(dut, column_name):
    out = dut(ovs_vsctl + "--format json list system", shell="bash")
    columns = json.loads(out)['headings']
    i = 0
    for column in columns:
        if column == column_name:
            return i
        i += 1
    return None


def read_os_release_file(dut, fname=default_os_release_file, key=None):
    """Read the local os-release file and return the data."""
    cur_dir, f = os.path.split(__file__)
    path = os.path.join(cur_dir, os_release_files_dir, fname)
    d = {}
    with open(path) as f:
        for line in f:
            k, v = line.rstrip().split("=")
            d[k] = v
    if key:
        return d[key]
    else:
        return d


def copy_os_release_file(dut, fname=default_os_release_file):
    """Copy a given os-release file to /etc/os-release."""
    # src = os.path.join(os.path.sep, 'shared', os_release_files_dir, fname)
    dst = os.path.join(os.path.sep, 'etc', 'os-release')
    dut("/bin/cp /tmp/files/os_releases/" + fname + " " + dst, shell="bash")


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

    dut(ovsdb_tool + "create /var/run/openvswitch/ovsdb.db "
        "/usr/share/openvswitch/vswitch.ovsschema", shell="bash")

    # Load the newly created DB into ovsdb-server
    dut(ovs_appctl + "-t ovsdb-server ovsdb-server/add-db "
        "/var/run/openvswitch/ovsdb.db", shell="bash")


def stop_ovsdb(dut):
    """Remove the OpenSwitch DB from ovsdb-server.

    It also removes the DB file from the file system.
    """

    # Remove the database from the ovsdb-server.
    dut(ovs_appctl + "-t ovsdb-server ovsdb-server/remove-db OpenSwitch",
        shell="bash")

    # Remove the DB file from the file system.
    dut("/bin/rm -f /var/run/openvswitch/ovsdb.db", shell="bash")


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


@pytest.fixture(scope="module")
def main_setup(request, topology):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    cur_dir, f = os.path.split(__file__)
    src = os.path.join(cur_dir, os_release_files_dir)
    dst = os.path.join(ops1.shared_dir, os_release_files_dir)
    shutil.copytree(src, dst)


@pytest.fixture()
def setup(request, topology):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    stop(ops1)

    def cleanup():
        stop(ops1)
        copy_os_release_file(ops1)
        start(ops1)

    request.addfinalizer(cleanup)


def test_sysd_ct_os_default_os_name(topology, step, main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    check_os_name(ops1, "os-release.default")


def test_sysd_ct_os_default_version(topology, step, main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    check_switch_version(ops1, "os-release.default")


def test_sysd_ct_os_1_0_0_version(topology, step, main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    check_switch_version(ops1, "os-release.ops-1.0.0")


def test_sysd_ct_os_debian_8_0_name(topology, step, main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    check_os_name(ops1, "os-release.debian-8.0")


def test_sysd_ct_os_debian_8_0_version(topology, step, main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    check_switch_version(ops1, "os-release.debian-8.0")
