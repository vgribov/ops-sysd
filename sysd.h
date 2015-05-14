/* vim: ai:ts=4:sw=4:expandtab
 */
/************************************************************************//**
 * @defgroup sysd Halon system daemon
 *
 * @brief Halon system daemon (sysd)
 *
 * The sysd daemon is responsible for detecting the presense of supported
 * subsystems on the switch platform and populating OVSDB.
 * This deamon's primary responsibilities include:
 *     - process the manifest file to populate platform specific information
 *       into the database.
 *     - parse hardware desciption files for the given OpenHalon platform.
 *     - detect subsystems and validate against hardware description files.
 *     - populate OVSDB interface table based on detected physical interfaces.
 *     - orchestrate daemon startup for daemons that interact with hardware.
 *
 * @{
 *
 * @file
 * Header for sysd daemon
 *
 * Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
 * All Rights Reserved.

 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at

 *         http://www.apache.org/licenses/LICENSE-2.0

 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *    License for the specific language governing permissions and limitations
 *    under the License.
 *
 * @defgroup sysd_public Public Interface
 * The sysd platform daemon manages the physical subsystems and physical
 * interfaces on the OpenHalon platform.
 *
 * @{
 *
 * Public APIs
 *
 * Command line options:
 *
 *     sysd: Halon system daemon
 *     usage: sysd [OPTIONS] [DATABASE]
 *     where DATABASE is a socket on which ovsdb-server is listening
 *           (default: "unix:/var/run/openvswitch/db.sock").
 *
 *      Daemon options:
 *        --detach                run in background as daemon
 *        --no-chdir              do not chdir to '/'
 *        --pidfile[=FILE]        create pidfile (default: /var/run/openvswitch/sysd.pid)
 *        --overwrite-pidfile     with --pidfile, start even if already running
 *
 *      Logging options:
 *        -vSPEC, --verbose=SPEC   set logging levels
 *        -v, --verbose            set maximum verbosity level
 *        --log-file[=FILE]        enable logging to specified FILE
 *                                 (default: /var/log/openvswitch/sysd.log)
 *        --syslog-target=HOST:PORT  also send syslog msgs to HOST:PORT via UDP
 *
 *      Other options:
 *        --unixctl=SOCKET        override default control socket name
 *        -h, --help              display this help message
 *
 *
 * Available ovs-apptcl command options are:
 *
 *      coverage/show
 *      exit
 *      list-commands
 *      version
 *      sysd/dump      dumps daemons internal data for debugging.
 *      vlog/disable-rate-limit [module]...
 *      vlog/enable-rate-limit  [module]...
 *      vlog/list
 *      vlog/reopen
 *      vlog/set                {spec | PATTERN:destination:pattern}
 *
 *
 * OVSDB elements usage
 *
 *  The following table rows are CREATED by sysd:
 *
 *      Interface row
 *      Subsystem row
 *
 *  The following columns are WRITTEN by sysd:
 *
 *      Open_vSwitch:subsystems
 *      Open_vSwitch:cur_hw
 *      Open_vSwitch:next_hw
 *
 *      Subsystem:name, asset_tag_number, hw_desc_dir, other_config, interfaces
 *
 *      Interface:name, hw_intf_info
 *
 *      Daemon: name, cur_hw, is_hw_handler
 *
 * Linux Files:
 *
 *  The following files are written by sysd:
 *
 *      /var/run/openvswitch/sysd.pid: Process ID for the sysd daemon
 *      /var/run/openvswitch/sysd.<pid>.ctl: Control file for ovs-appctl
 *
 ***************************************************************************/
/** @} end of group sysd_public */

#ifndef __SYSD_H__
#define __SYSD_H__

#include <stdint.h>
#include "sysd_fru.h"

#define DFLT_ASSET_TAG             "Open Halon asset tag"
#define BASE_SUBSYSTEM             "base"
#define SYSD_BASE_SUBSYSTEM        "base"

#define SYSD_SUBSYSTEM_TYPE_UNINIT      "uninitialized"
#define SYSD_SUBSYSTEM_TYPE_MEZZ        "mezz_card"
#define SYSD_SUBSYSTEM_TYPE_LINE        "line_card"
#define SYSD_SUBSYSTEM_TYPE_CHASSIS     "chassis"
#define SYSD_SUBSYSTEM_TYPE_SYSTEM      "system"

/**
 * Max supported split ports on QSFP ports.
 */
/* HALON_TODO This MACRO should go to a common header file. */
#define SYSD_MAX_SPLIT_PORTS      4

#define MAX_SUBSYSTEM_NAME_LEN    512

typedef YamlPortInfo sysd_intf_cmn_info_t;
typedef YamlPort     sysd_intf_info_t;

/*************************************************************************//**
 * sysd's internal data structure to store per subsytem data.
 ****************************************************************************/
typedef struct subsystem {
    char                    name[MAX_SUBSYSTEM_NAME_LEN];
    const char              *type;
    bool                    valid;
    int                     intf_count;         /*!< Total number of interfaces. */
    sysd_intf_cmn_info_t    *intf_cmn_info;     /*!< Global info about interfaces. */
    sysd_intf_info_t        **interfaces;       /*!< Per interface info. */

    fru_eeprom_t            fru_eeprom;

    uint64_t                nxt_mac_addr;       /*!< Next avaliable MAC addr */
    int                     num_free_macs;
} sysd_subsystem_t;

extern struct ovsdb_idl  *idl;
extern uint32_t          idl_seqno;
extern int               num_subsystems;
extern sysd_subsystem_t  **subsystems;

#endif /* __SYSD_H__ */

/** @} end of group sysd */
