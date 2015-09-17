#!/usr/bin/python
#
# Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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

import os
import sys
import time
import subprocess
import pytest
import shutil
import json
import yaml

from opsvsi.docker import *
from opsvsi.opsvsitest import *

OVS_VSCTL = "/usr/bin/ovs-vsctl "
OVS_APPCTL = "/usr/bin/ovs-appctl "
OVSDB_CLIENT = "/usr/bin/ovsdb-client "
OVSDB_TOOL = "/usr/bin/ovsdb-tool "

HW_DESC_FILES_DIR = "test_hw_desc_files"
IMAGE_MANIFEST_FILES_DIR = "test_manifest_files"

DUT_IMAGE_MANIFEST_FILES_DIR = "/shared/test_manifest_files/"
DUT_HW_DESC_FILES_DIR = "/shared/test_hw_desc_files/"

SYSTEM_IMAGE_MANIFEST_FILE = "/etc/openswitch/image.manifest"
SYSTEM_HW_DESC_FILES_DIR = "/etc/openswitch/hwdesc"


def short_sleep(tm=.5):
    time.sleep(tm)

# Remove the OpenSwitch DB from ovsdb-server.
# Also remove the DB file from the file system.
def ovsdb_stop_openswitchDB(sw):
    # Remove the database from the ovsdb-server.
    sw.cmd("/usr/bin/ovs-appctl -t ovsdb-server ovsdb-server/remove-db OpenHalon")

    # Remove the DB file from the file system.
    sw.cmd("/bin/rm -f /var/run/openvswitch/ovsdb.db")

# Create a empty DB file and load it into ovsdb-server
def ovsdb_start_openswitchDB(sw):
    # Create a empty database file.
    c = "/usr/bin/ovsdb-tool create " \
            "/var/run/openvswitch/ovsdb.db " \
            "/usr/share/openvswitch/vswitch.ovsschema"
    sw.cmd(c)

    # Load the newly create DB into ovsdb-server
    sw.cmd("/usr/bin/ovs-appctl -t ovsdb-server ovsdb-server/add-db /var/run/openvswitch/ovsdb.db")

# Wait until System table is visible in the
# ovsdb-server.
def wait_until_ovsdb_is_up(sw):
    cmd = OVS_VSCTL + "list System | grep uuid"
    wait_count = 20
    while wait_count > 0:
        out = sw.ovscmd(cmd)
        if "_uuid" in out:
            break

        info(out)
        wait_count -= 1
        short_sleep(1)

    if wait_count == 0:
        info("Failed to bring up ovsdb-server.")
        CLI(Test_sysd_startup.test.net)

# Stop sysd
def sysd_stop(sw):
    sw.cmd("/usr/bin/ovs-appctl -t sysd exit")

# Start sysd
def sysd_start(sw):
    sw.cmd("/bin/systemctl start sysd")

# Restart sysd
def sysd_restart(sw):
    sysd_stop(sw)
    sysd_start(sw)

# Copy a given image.manifest file to "/etc/openswitch/image.manifest"
def copy_image_manifest_file(sw, file_name):
    c = "/bin/cp " + DUT_IMAGE_MANIFEST_FILES_DIR + file_name + \
        " " + SYSTEM_IMAGE_MANIFEST_FILE
    sw.cmd(c)

# Copy Hardware description files to /etc/openswitch/hwdesc
def copy_hw_desc_files(sw):
    c = "/bin/cp -a " + DUT_HW_DESC_FILES_DIR + "/*.yaml " + \
        SYSTEM_HW_DESC_FILES_DIR
    sw.cmd(c)

# Get daemon table from ovsdb-server
def ovs_vsctl_daemons_list(sw):
    daemon_list = {}
    c = OVS_VSCTL + " --format json list daemon"
    out = sw.cmd(c)
    json_out = json.loads(out)['data']

    # The output is in the following format
    # [["uuid","19b943b0-096c-4d7c-bc0c-5b6ac2f83014"],0,true,"pmd"]
    for item in json_out:
        daemon_list[item[3]] = {'is_hw_handler' : item[2]}

    return daemon_list

# Read the local image.manifest file and return the data.
def load_image_manifest_file(file_name):
    cur_dir, f = os.path.split(__file__)
    json_file_loc = cur_dir + "/" + IMAGE_MANIFEST_FILES_DIR + "/" + file_name
    debug("Loading the YAML file:" + json_file_loc + "\n")

    json_data=open(json_file_loc).read()
    return json.loads(json_data)['daemons']

# Read the local port.yaml file and return the data.
def load_yaml_port_file(file_name):
    cur_dir, f = os.path.split(__file__)
    yaml_file_loc = cur_dir + "/" + HW_DESC_FILES_DIR + "/" + file_name
    debug("Loading the image manifest file:" + yaml_file_loc + "\n")

    yaml_data=open(yaml_file_loc, "r")
    return yaml.load(yaml_data)

# Get the value for a given key from subsystem table other_info column.
def get_subsystem_other_info(sw, uuid, map_key):
    uuid = uuid.replace('\r\n','')
    c = OVS_VSCTL + " get subsystem " + uuid + " other_info:" + map_key
    out = sw.ovscmd(c)
    return out.replace('\r\n','')

# Get the value for a given key from interface table hw_intf_info column.
def get_interface_hw_info(sw, name, map_key):
    c = OVS_VSCTL + " get interface " + str(name) + " hw_intf_info:" + map_key
    out = sw.ovscmd(c)
    return out.replace('\r\n','')


class sysdTest(OpsVsiTest):

    def setupNet(self):
        # Create a topology with single openswitch.
        switch_opts = self.getSwitchOpts()
        intfd_topo = SingleSwitchTopo(k=1, sopts=switch_opts)
        self.net = Mininet(intfd_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)


    def image_manifest_read(self, file_name):

        info("\n=== SYSd image manifest read test. File: %s ===\n" % file_name)
        s1 = self.net.switches[0]

        # Stop sysd & ovsdb-server (unload the database also).
        sysd_stop(s1)
        ovsdb_stop_openswitchDB(s1)
        short_sleep(3)

        # Copy a new image.manifest file to the switch
        copy_image_manifest_file(s1, file_name)

        # Restart the ovsdb-server and sysd
        ovsdb_start_openswitchDB(s1)
        short_sleep(3)

        sysd_start(s1)

        wait_until_ovsdb_is_up(s1)

        # Make sure that the data in ovsdb-server matches
        # the local file.
        ovsdb_daemons_list = ovs_vsctl_daemons_list(s1)
        file_daemons_list = load_image_manifest_file(file_name)

        assert cmp(ovsdb_daemons_list, file_daemons_list) == 0, \
               "OVSDB has incorrect image.manifest info."


    def test_sysd_hw_desc_files_read(self):

        info("\n=== SYSd H/W description files read test. ===\n")
        s1 = self.net.switches[0]

        # Stop sysd & ovsdb-server (unload the database also).
        sysd_stop(s1)
        ovsdb_stop_openswitchDB(s1)
        short_sleep(3)

        # Copy new hardware description files.
        copy_hw_desc_files(s1)

        # Restart ovsdb-server and sysd.
        ovsdb_start_openswitchDB(s1)
        short_sleep(3)

        sysd_start(s1)

        wait_until_ovsdb_is_up(s1)

        hw_desc_data = load_yaml_port_file("ports.yaml")

        susytem_uuid = s1.ovscmd("ovs-vsctl list subsystem | grep -i uuid | cut -d : -f 2")

        port_count = get_subsystem_other_info(s1, susytem_uuid, "interface_count")
        assert port_count == str(hw_desc_data['port_info']['number_ports']), \
               "Number of ports in OVSDB is incorrect."

        lag_count = get_subsystem_other_info(s1, susytem_uuid, "max_bond_count")
        assert lag_count == str(hw_desc_data['port_info']['max_lag_count']), \
               "Maximum number of lags in OVSDB is incorrect."

        lag_member_count = get_subsystem_other_info(s1, susytem_uuid, "max_bond_member_count")
        assert lag_member_count == str(hw_desc_data['port_info']['max_lag_member_count']), \
               "Maximum number of members in a lag in OVSDB is incorrect."

        # Match the port hw_intf_info details.
        port_name = hw_desc_data['ports'][0]['name']

        sw_dev_port = get_interface_hw_info(s1, port_name, "switch_intf_id")
        assert sw_dev_port == str(hw_desc_data['ports'][0]['switch_device_port']), \
               "Interface hardware info:switch_intf_id is incorrect."

        connector_type = get_interface_hw_info(s1, port_name, "connector")
        assert connector_type == str(hw_desc_data['ports'][0]['connector']), \
               "Interface hardware info:connector is incorrect."

        # Make sure default bridge & default VRF are created.
        dflt_bridge = s1.ovscmd("/usr/bin/ovs-vsctl get bridge bridge_normal name")
        dflt_bridge = dflt_bridge.replace('\r\n','')
        assert dflt_bridge == "bridge_normal", \
               "Default bridge is not created."

        dflt_vrf = s1.ovscmd("/usr/bin/ovs-vsctl get vrf vrf_default name")
        dflt_vrf = dflt_vrf.replace('\r\n','')
        assert dflt_vrf == "vrf_default", \
               "Default vrf is not created."


class Test_sysd_startup:

    def setup_class(cls):
        # Create the Mininet topology based on mininet.
        Test_sysd_startup.test = sysdTest()

        # Copy the test files (image.manifest, hw_descr files)
        # from current directory to switch shared directory.
        s1 = Test_sysd_startup.test.net.switches[0]
        cur_dir, f = os.path.split(__file__)

        info("Copying test image manifest files to switch shared directory.\n")
        test_file_dir = os.path.join(cur_dir, HW_DESC_FILES_DIR)
        shutil.copytree(test_file_dir, s1.shareddir + "/" + HW_DESC_FILES_DIR)

        info("Copying test hw descr files to switch shared directory.\n")
        test_file_dir = os.path.join(cur_dir, IMAGE_MANIFEST_FILES_DIR)
        shutil.copytree(test_file_dir, s1.shareddir + "/" + IMAGE_MANIFEST_FILES_DIR)

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_sysd_startup.test.net.stop()

    def __del__(self):
        del self.test

    # sysd component tests.
    def test_sysd_image_manifest_read(self):
        self.test.image_manifest_read("image.manifest1")
        self.test.image_manifest_read("image.manifest2")
        self.test.image_manifest_read("image.manifest3")
        self.test.image_manifest_read("image.manifest")

    def test_sysd_hw_desc_files_read(self):
        self.test.test_sysd_hw_desc_files_read()
