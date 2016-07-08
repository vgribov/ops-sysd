# -*- coding: utf-8 -*-
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
OpenSwitch Test for sysd related configurations.
"""

from pytest import mark
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

image_manifest_files_dir = "test_manifest_files"
system_image_manifest_files = "/etc/openswitch/image.manifest"


def image_manifest_read(dut, file_name):
    """Testing ops-sysd correctly parse image manifest file

    Test if the ops-sysd correctly parse the image manifest file and stores
    the information in the OVSDB.
    """
    # Copy a new image.manifest file to the switch
    copy_image_manifest_file(dut, file_name)

    # Restart the ovsdb-server and sysd
    start(dut)

    # Make sure that the data in ovsdb-server matches the local file.
    ovsdb_daemons_list = list_daemons(dut)
    file_daemons_list = read_image_manifest_file(dut, file_name)
    print(ovsdb_daemons_list)
    print(file_daemons_list)
    assert ovsdb_daemons_list == file_daemons_list


def list_daemons(dut):
    """Get daemon table from ovsdb-server."""
    daemon_list = {}
    c = ovs_vsctl + "--format json list daemon"
    out = dut(c, shell="bash")
    json_out = json.loads(out)['data']

    # The output is in the following format
    # [["uuid","19b943b0-096c-4d7c-bc0c-5b6ac2f83014"],0,true,"ops-pmd"]
    for item in json_out:
        daemon_list[item[3]] = {'is_hw_handler': item[2]}

    return daemon_list


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


def copy_image_manifest_file(dut, file_name):
    """
    Copy a given image.manifest file to /etc/openswitch/image.manifest
    """
    dut("/bin/cp /tmp/test_manifest_files/" + file_name +
        " " + system_image_manifest_files, shell="bash")


def read_image_manifest_file(dut, file_name):
    """Read the local image.manifest file and return the data."""
    cur_dir, f = os.path.split(__file__)
    json_file_loc = (cur_dir + "/" + image_manifest_files_dir +
                     "/" + file_name)

    json_data = open(json_file_loc).read()
    return json.loads(json_data)['daemons']


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
    test_file_dir = os.path.join(cur_dir, image_manifest_files_dir)
    shutil.copytree(test_file_dir, ops1.shared_dir + "/" +
                    image_manifest_files_dir)


@pytest.fixture()
def setup(request, topology):
    ops1 = topology.get('ops1')

    assert ops1 is not None

    stop(ops1)

    def cleanup():
        stop(ops1)
        copy_image_manifest_file(ops1, "image.manifest")
        start(ops1)

    request.addfinalizer(cleanup)


@mark.gate
def test_sysd_ct_image_change_hw_handler_to_false(topology, step, main_setup,
                                                  setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    image_manifest_read(ops1, "image.manifest1")


@mark.gate
def test_sysd_ct_image_change_mgmt_intf_from_eth0_to_mgmt1(topology, step,
                                                           main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    image_manifest_read(ops1, "image.manifest2")


@mark.gate
def test_sysd_ct_image_add_random_stuff(topology, step, main_setup, setup):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    image_manifest_read(ops1, "image.manifest3")
