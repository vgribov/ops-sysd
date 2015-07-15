/* vim: ai:ts=4:sw=4:expandtab
 */
/************************************************************************//**
 * @ingroup sysd
 *
 * @file
 * Source for sysd OVSDB access interface.
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
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <daemon.h>
#include <dirs.h>
#include <smap.h>
#include <shash.h>
#include <poll-loop.h>
#include <ovsdb-idl.h>
#include <openhalon-idl.h>
#include <vswitch-idl.h>
#include <openvswitch/vlog.h>

#include <hc-utils.h>
#include <config-yaml.h>
#include "sysd.h"
#include "sysd_util.h"
#include "sysd_ovsdb_if.h"

VLOG_DEFINE_THIS_MODULE(ovsdb_if);

/** @ingroup sysd
 * @{ */

extern char *g_hw_desc_dir;

static bool hw_init_done_set = false;

void
sysd_get_speeds_string(char *speed_str, int len, int **speeds)
{
    int     i = 0;
    char    buf[10];

    while(speeds[i] != NULL) {
        if (i == 0) {
            snprintf(buf, sizeof(buf), "%d", *speeds[i]);
        } else {
            snprintf(buf, sizeof(buf), ",%d", *speeds[i]);
        }
        strncat(speed_str, buf, len);
        i++;
    }
} /* sysd_get_speeds_string */

struct ovsrec_interface *
sysd_initial_interface_add(struct ovsdb_idl_txn *txn,
                           sysd_subsystem_t *subsys_ptr,
                           sysd_intf_info_t *intf_ptr)
{
    char                        *tmp_p;
    char                        buf[128];
    struct ovsrec_interface     *ovs_intf = NULL;
    struct smap                 hw_intf_info;
    char                        **cap_p;

    ovs_intf = ovsrec_interface_insert(txn);

    ovsrec_interface_set_name(ovs_intf, intf_ptr->name);

    ovsrec_interface_set_admin_state(ovs_intf, OVSREC_INTERFACE_ADMIN_STATE_DOWN);

    smap_init(&hw_intf_info);

    tmp_p = (intf_ptr->pluggable) ? INTERFACE_HW_INTF_INFO_MAP_PLUGGABLE_TRUE
        : INTERFACE_HW_INTF_INFO_MAP_PLUGGABLE_FALSE;
    smap_add(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_PLUGGABLE, tmp_p);
    smap_add(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_CONNECTOR, intf_ptr->connector);

    smap_add_format(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_MAX_SPEED,
                    "%d", intf_ptr->max_speed);

    memset(buf, 0, sizeof(buf));
    sysd_get_speeds_string(buf, sizeof(buf), intf_ptr->speeds);
    smap_add(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_SPEEDS, buf);


    smap_add_format(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_SWITCH_UNIT,
                    "%d", intf_ptr->device);
    smap_add_format(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_SWITCH_INTF_ID,
                    "%d", intf_ptr->device_port);

    /* Add interface capabilities
     * Check for known values and add them. If an unknown capability is given,
     * log (info) it and go ahead and add it.
    */
    cap_p = intf_ptr->capabilities;

    while (*cap_p != (char *) NULL) {
        if ((strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_SPLIT_4) != 0)  &&
            (strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_ENET1G)  != 0)  &&
            (strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_ENET10G) != 0)  &&
            (strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_ENET40G) != 0)) {

            VLOG_INFO("subsystem[%s]:interface[%s] - adding unknown "
                      "interface capability[%s]",
                             subsys_ptr->name, intf_ptr->name, *cap_p);
        }

        smap_add(&hw_intf_info, *cap_p, "true");
        cap_p++;
    }

    if (subsys_ptr->num_free_macs) {
        memset(buf, 0, sizeof(buf));
        tmp_p = hc_ether_ulong_long_to_string(buf, subsys_ptr->nxt_mac_addr);
        smap_add(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_MAC_ADDR, tmp_p);

        subsys_ptr->num_free_macs--;
        subsys_ptr->nxt_mac_addr++;
    }

    ovsrec_interface_set_hw_intf_info(ovs_intf, &hw_intf_info);
    smap_destroy(&hw_intf_info);

    /*
     * HALON_TODO:
     * Current schema has in-correct values for interface table.
     * Once we fix all that, need to fix the following code.
     *
     * unsigned int                speed_count = 0;
     * ovsrec_interface_set_connector(ovs_intf, intf_ptr->connector);
     * ovsrec_interface_set_max_speed(ovs_intf, &(intf_ptr->max_speed), 1);
     * speed_count = get_count_null_ended((void *) intf_ptr->speeds);
     * ovsrec_interface_set_link_speed(ovs_intf, intf_ptr->speeds, speed_count); */

    return ovs_intf;

} /* sysd_initial_interface_add */

void
sysd_set_splittable_port_info(struct ovsrec_interface **ovs_intf, sysd_subsystem_t *subsys_ptr)
{
    int                 i = 0;
    sysd_intf_info_t    *intf_ptr = NULL;
    struct shash        ovs_intf_shash = SHASH_INITIALIZER(&ovs_intf_shash);

    for (i = 0; i < subsys_ptr->intf_count; i++) {
        intf_ptr = subsys_ptr->interfaces[i];

        if ((intf_ptr->subports[0] != NULL) ||
            (intf_ptr->parent_port != NULL)) {

            shash_add(&ovs_intf_shash, ovs_intf[i]->name, ovs_intf[i]);
        }
    }

    for (i = 0; i < subsys_ptr->intf_count; i++) {
        intf_ptr = subsys_ptr->interfaces[i];

        if (intf_ptr->parent_port != NULL) {
            struct ovsrec_interface *parent = NULL;

            parent = shash_find_data(&ovs_intf_shash, intf_ptr->parent_port);
            if (parent != NULL) {
                ovsrec_interface_set_split_parent(ovs_intf[i], parent);
            } else {
                VLOG_WARN("Unable to find parent port %s of subport %s",
                          intf_ptr->parent_port, intf_ptr->name);
            }
        }

        if (intf_ptr->subports[0] != NULL) {
            struct ovsrec_interface *subports[SYSD_MAX_SPLIT_PORTS];
            int j = 0, k = 0;

            while(intf_ptr->subports[k] != NULL) {
                subports[j] = shash_find_data(&ovs_intf_shash, intf_ptr->subports[k]);
                if (subports[j] == NULL) {
                    VLOG_WARN("Unable to find subport %s of port %s",
                              intf_ptr->subports[k], intf_ptr->name);
                } else {
                    j++;
                }
                k++;
            }
            ovsrec_interface_set_split_children(ovs_intf[i], subports, j);
        }
    }
    shash_destroy(&ovs_intf_shash);

} /* sysd_set_splittable_port_info */

struct ovsrec_daemon *
sysd_initial_daemon_add(struct ovsdb_idl_txn *txn, daemon_info_t *daemon_ptr)
{
    struct ovsrec_daemon     *ovs_daemon = NULL;

    ovs_daemon = ovsrec_daemon_insert(txn);

    ovsrec_daemon_set_name(ovs_daemon, daemon_ptr->name);
    ovsrec_daemon_set_cur_hw(ovs_daemon, daemon_ptr->cur_hw);
    ovsrec_daemon_set_is_hw_handler(ovs_daemon, daemon_ptr->is_hw_handler);

    return ovs_daemon;

} /* sysd_initial_daemon_add */

struct ovsrec_subsystem *
sysd_initial_subsystem_add(struct ovsdb_idl_txn *txn, sysd_subsystem_t *subsys_ptr)
{
    int                         i = 0;
    fru_eeprom_t                *fru = NULL;

    struct smap                 other_info;
    struct ovsrec_subsystem     *ovs_subsys = NULL;
    struct ovsrec_interface     **ovs_intf = NULL;

    ovs_subsys = ovsrec_subsystem_insert(txn);

    fru = &(subsys_ptr->fru_eeprom);

    ovsrec_subsystem_set_name(ovs_subsys, subsys_ptr->name);
    ovsrec_subsystem_set_asset_tag_number(ovs_subsys, DFLT_ASSET_TAG);
    ovsrec_subsystem_set_hw_desc_dir(ovs_subsys, g_hw_desc_dir);

    smap_init(&other_info);

    smap_add(&other_info, "country_code", fru->country_code);
    smap_add_format(&other_info, "device_version", "%c", fru->device_version);
    smap_add(&other_info, "diag_version", fru->diag_version);
    smap_add(&other_info, "label_revision", fru->label_revision);
    smap_add_format(&other_info, "base_mac_address",
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    SYSD_MAC_FORMAT(fru->base_mac_address));
    smap_add_format(&other_info, "number_of_macs", "%d", fru->num_macs);
    smap_add(&other_info, "manufacturer", fru->manufacturer);
    smap_add(&other_info, "manufacture_date", fru->manufacture_date);
    smap_add(&other_info, "onie_version", fru->onie_version);
    smap_add(&other_info, "part_number", fru->part_number);
    smap_add(&other_info, "Product Name", fru->product_name);
    smap_add(&other_info, "platform_name", fru->platform_name);
    smap_add(&other_info, "serial_number", fru->serial_number);
    smap_add(&other_info, "vendor", fru->vendor);

    smap_add_format(&other_info, "interface_count",
                    "%d", subsys_ptr->intf_cmn_info->number_ports);
    smap_add_format(&other_info, "max_interface_speed",
                    "%d", subsys_ptr->intf_cmn_info->max_port_speed);
    smap_add_format(&other_info, "max_transmission_unit",
                    "%d", subsys_ptr->intf_cmn_info->max_transmission_unit);
    smap_add_format(&other_info, "max_bond_count",
                    "%d", subsys_ptr->intf_cmn_info->max_lag_count);
    smap_add_format(&other_info, "max_bond_member_count",
                    "%d", subsys_ptr->intf_cmn_info->max_lag_member_count);

    ovsrec_subsystem_set_other_info(ovs_subsys, &other_info);
    smap_destroy(&other_info);

    ovs_intf = SYSD_OVS_PTR_CALLOC(ovsrec_interface *, subsys_ptr->intf_count);
    if (ovs_intf == NULL) {
        VLOG_ERR("Failed to allocate memory for OVS subsystem interfaces.");
        return ovs_subsys;
    }

    for (i = 0; i < subsys_ptr->intf_count; i++) {
        ovs_intf[i] = sysd_initial_interface_add(txn, subsys_ptr, subsys_ptr->interfaces[i]);
    }

    sysd_set_splittable_port_info(ovs_intf, subsys_ptr);

    ovsrec_subsystem_set_interfaces(ovs_subsys, ovs_intf, i);

    return ovs_subsys;

} /* sysd_initial_subsystem_add */

/*
 * This function is used to initialize the default VRF during system bootup.
 */
void
sysd_configure_default_vrf(struct ovsdb_idl_txn *txn,
                           struct ovsrec_open_vswitch *ovs_row)
{
    struct ovsrec_vrf *default_vrf_row = NULL;

    default_vrf_row = ovsrec_vrf_insert(txn);
    ovsrec_vrf_set_name(default_vrf_row, OVSDB_VRF_DEFAULT_NAME);
    ovsrec_open_vswitch_set_vrfs(ovs_row, &default_vrf_row, 1);

}/* sysd_configure_default_vrf */

void
sysd_initial_configure(struct ovsdb_idl_txn *txn)
{
    int     i = 0;

    struct ovsrec_daemon **ovs_daemon_l = NULL;

    struct ovsrec_subsystem **ovs_subsys_l = NULL;
    struct ovsrec_open_vswitch *ovs_vsw = NULL;

    ovs_vsw = ovsrec_open_vswitch_insert(txn);

    ovs_subsys_l = SYSD_OVS_PTR_CALLOC(ovsrec_subsystem *, num_subsystems);
    if (ovs_subsys_l == NULL) {
        VLOG_ERR("Failed to allocate memory for OVS subsystem.");
        return;
    }

    for (i = 0; i < num_subsystems; i++) {
        ovs_subsys_l[i] = sysd_initial_subsystem_add(txn, subsystems[i]);
    }

    ovsrec_open_vswitch_set_subsystems(ovs_vsw, ovs_subsys_l, num_subsystems);

    sysd_configure_default_vrf(txn, ovs_vsw);

    ovs_daemon_l = SYSD_OVS_PTR_CALLOC(ovsrec_daemon *, num_daemons);
    if (ovs_daemon_l == NULL) {
        VLOG_ERR("Failed to allocate memory for OVS daemon table.");
        return;
    }

    /* Add the daemon info to the daemon table */
    if (num_daemons > 0) {
        for (i = 0; i < num_daemons; i++) {
            ovs_daemon_l[i] = sysd_initial_daemon_add(txn, daemons[i]);
        }

        ovsrec_open_vswitch_set_daemons(ovs_vsw, ovs_daemon_l, num_daemons);
    }

} /* sysd_initial_configure */

static void
sysd_set_hw_done(void)
{
    struct ovsdb_idl_txn                *txn = NULL;
    const struct ovsrec_open_vswitch    *ovs_vsw = NULL;
    enum ovsdb_idl_txn_status           txn_status = TXN_ERROR;

    txn = ovsdb_idl_txn_create(idl);

    OVSREC_OPEN_VSWITCH_FOR_EACH(ovs_vsw, idl) {
        ovsrec_open_vswitch_set_cur_hw(ovs_vsw, (int64_t) 1);
        ovsrec_open_vswitch_set_next_hw(ovs_vsw, (int64_t) 1);
    }

    txn_status = ovsdb_idl_txn_commit_block(txn);
    if (txn_status != TXN_SUCCESS) {
        VLOG_ERR("Failed to set cur_hw, next_hw = 1. rc = %u", txn_status);
    }
    ovsdb_idl_txn_destroy(txn);

    hw_init_done_set = true;

    VLOG_INFO("H/W description file processing completed");

} /* sysd_set_hw_done() */

static void
sysd_chk_if_hw_daemons_done(void)
{
    int i;
    int not_set = false;
    int num_found = 0;

    const struct ovsrec_daemon *db_daemon;

    /*
     * There are several platform daemons, of which some read the h/w
     * description files and put information in the db. The ovsdb Daemon
     * table lists daemons for the system. The minimum set listed are those
     * platform daemons that read and process the h/w description files.
     * Each of these are called "h/w daemons".
     *
     * Each h/w daemon is responsible to set...
     *      Daemon["<their name>"]:cur_hw = 1
     * after they have completed processing the h/w description files.
     *
     * Sysd will watch for all of these daemons to register that they
     * have completed their processing and will then update...
     *      Open_vSwitch:{cur_hw,next_hw} = 1.
     *
     * The configuration daemon waits for sysd to set Open_vSwitch:cur_hw=1
     * before it tries to push anything into the db, to ensure that all h/w
     * processing is done before any user configuration is pushed.
    */

    if (num_hw_daemons <= 0) {
        sysd_set_hw_done();
        return;
    }

    /* See if all h/w daemons have set cur_hw > 0 */
    for (i = 0; i < num_daemons; i++) {
        if (daemons[i]->is_hw_handler) {
            OVSREC_DAEMON_FOR_EACH(db_daemon, idl) {
                if (db_daemon->is_hw_handler) {
                    if (strncmp(daemons[i]->name, db_daemon->name,
                                strlen(daemons[i]->name)) == 0) {
                        if (db_daemon->cur_hw > 0) {
                            num_found++;
                        } else {
                            not_set = true;
                        }
                        break;
                    }
                }
            }
        }
        if (not_set) {
            break;
        }
    }

    /* Not all set, try again later. */
    if (not_set || (num_found == 0)) return;

    /* All are set. Now set system table cur_hw, next_hw = 1 */
    sysd_set_hw_done();

    return;

} /* sysd_chk_if_hw_daemons_done() */

void
sysd_run(void)
{
    uint32_t                            new_seqno = 0;
    enum ovsdb_idl_txn_status           txn_status = TXN_ERROR;
    struct ovsdb_idl_txn                *txn = NULL;
    const struct ovsrec_open_vswitch    *cfg = NULL;

    ovsdb_idl_run(idl);

    if (ovsdb_idl_is_lock_contended(idl)) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

        VLOG_ERR_RL(&rl, "another ovs-vswitchd process is running, "
                    "disabling this process (pid %ld) until it goes away",
                    (long int) getpid());

        return;
    } else if (!ovsdb_idl_has_lock(idl)) {
        return;
    }

    new_seqno = ovsdb_idl_get_seqno(idl);
    if (new_seqno != idl_seqno) {

        idl_seqno = ovsdb_idl_get_seqno(idl);

        cfg = ovsrec_open_vswitch_first(idl);

        if (cfg == NULL) {
            txn = ovsdb_idl_txn_create(idl);

            sysd_initial_configure(txn);

            txn_status = ovsdb_idl_txn_commit_block(txn);
            if (txn_status != TXN_SUCCESS) {
                VLOG_ERR("Failed to commit the transaction. rc = %u", txn_status);
            }
            ovsdb_idl_txn_destroy(txn);
        } else if (!hw_init_done_set) {
            sysd_chk_if_hw_daemons_done();
        }
    }

    /* Notify parent of startup completion. */
    daemonize_complete();

} /* sysd_run */

void
sysd_wait(void)
{
    ovsdb_idl_wait(idl);

} /* sysd_wait */
/** @} end of group sysd */
