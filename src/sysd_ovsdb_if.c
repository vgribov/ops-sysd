/************************************************************************//**
 * (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *    License for the specific language governing permissions and limitations
 *    under the License.
 *
 *  File: sysd_ovsdb_if.c
 *
 ***************************************************************************/
/* @ingroup sysd
 *
 * @file
 * Source for sysd OVSDB access interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#include <daemon.h>
#include <dirs.h>
#include <smap.h>
#include <shash.h>
#include <poll-loop.h>
#include <ovsdb-idl.h>
#include <openswitch-idl.h>
#include <vswitch-idl.h>
#include <openvswitch/vlog.h>

#include <ops-utils.h>
#include <vrf-utils.h>
#include <config-yaml.h>
#include <yaml.h>
#include "qos_init.h"
#include "acl_init.h"
#include "sysd.h"
#include "sysd_util.h"
#include "sysd_ovsdb_if.h"
#include "eventlog.h"

#include <errno.h>


VLOG_DEFINE_THIS_MODULE(ovsdb_if);

/** @ingroup sysd
 * @{ */
#define PKG_INFO_ENTRIES_PER_COMMIT 2000
#define REM_BUF_LEN (buflen - 1 - strlen(buf))

enum {
    VALUE,
    PKG,
    PV,
    SRCREV,
    SRC_URL,
    TYPE,
    MAX_NUM_KEYS
};

const char * keystr_pkg_info[MAX_NUM_KEYS] = {
    "values",
    "PKG",
    "PV",
    "SRCREV",
    "SRC_URL",
    "TYPE",
};

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

    ovsrec_interface_set_type(ovs_intf, OVSREC_INTERFACE_TYPE_SYSTEM);

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
            (strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_ENET25G) != 0)  &&
            (strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_ENET40G) != 0)  &&
            (strcmp(*cap_p, INTERFACE_HW_INTF_INFO_MAP_ENET100G) != 0)) {

            VLOG_INFO("subsystem[%s]:interface[%s] - adding unknown "
                      "interface capability[%s]",
                             subsys_ptr->name, intf_ptr->name, *cap_p);
        }

        smap_add(&hw_intf_info, *cap_p, "true");
        cap_p++;
    }

    /* All the interfaces in a subsystem uses the same MAC address.
     * Copy the subsystem system MAC to interface hw_info:mac_addres.
     */
    if (subsys_ptr->system_mac_addr) {
        memset(buf, 0, sizeof(buf));
        tmp_p = ops_ether_ulong_long_to_string(buf, subsys_ptr->system_mac_addr);
        smap_add(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_MAC_ADDR, tmp_p);
    }

    ovsrec_interface_set_hw_intf_info(ovs_intf, &hw_intf_info);
    smap_destroy(&hw_intf_info);

    /*
     * OPS_TODO:
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
    char                        mac_addr[32];
    char                        *tmp_p;

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
    smap_add(&other_info, "diag_version", fru->diag_version ? : "");
    smap_add(&other_info, "label_revision", fru->label_revision ? : "");
    smap_add_format(&other_info, "base_mac_address",
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    SYSD_MAC_FORMAT(fru->base_mac_address));
    smap_add_format(&other_info, "number_of_macs", "%d", fru->num_macs);
    smap_add(&other_info, "manufacturer", fru->manufacturer ? : "");
    smap_add(&other_info, "manufacture_date", fru->manufacture_date ? : "");
    smap_add(&other_info, "onie_version", fru->onie_version ? : "");
    smap_add(&other_info, "part_number", fru->part_number ? : "");
    smap_add(&other_info, "Product Name", fru->product_name ? : "");
    smap_add(&other_info, "platform_name", fru->platform_name ? : "");
    smap_add(&other_info, "serial_number", fru->serial_number ? : "");
    smap_add(&other_info, "vendor", fru->vendor ? : "");

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
    smap_add_format(&other_info, "l3_port_requires_internal_vlan",
                    "%d", subsys_ptr->intf_cmn_info->l3_port_requires_internal_vlan);

    ovsrec_subsystem_set_other_info(ovs_subsys, &other_info);
    smap_destroy(&other_info);

    ovs_intf = SYSD_OVS_PTR_CALLOC(ovsrec_interface *, subsys_ptr->intf_count);
    if (ovs_intf == NULL) {
        VLOG_ERR("Failed to allocate memory for OVS subsystem interfaces.");
        log_event("SYS_ALLOCATE_MEMORY_FAILURE", EV_KV("value",
            "%s", "OVS subsystem interfaces"));
        return ovs_subsys;
    }

    for (i = 0; i < subsys_ptr->intf_count; i++) {
        ovs_intf[i] = sysd_initial_interface_add(txn, subsys_ptr, subsys_ptr->interfaces[i]);
    }

    sysd_set_splittable_port_info(ovs_intf, subsys_ptr);

    /* Save next_mac_address and macs_remaining in subsystem */
    memset(mac_addr, 0, sizeof(mac_addr));
    tmp_p = ops_ether_ulong_long_to_string(mac_addr, subsys_ptr->nxt_mac_addr);
    ovsrec_subsystem_set_next_mac_address(ovs_subsys, tmp_p);
    ovsrec_subsystem_set_macs_remaining(ovs_subsys, subsys_ptr->num_free_macs);

    ovsrec_subsystem_set_interfaces(ovs_subsys, ovs_intf, i);

    free(ovs_intf);

    return ovs_subsys;

} /* sysd_initial_subsystem_add */

/*
 * This function is used to initialize the default bridge during system bootup.
 */
void
sysd_configure_default_bridge(struct ovsdb_idl_txn *txn,
                              struct ovsrec_system *ovs_row)
{
    struct ovsrec_bridge *default_bridge_row = NULL;
    struct ovsrec_port *port = NULL;
    struct ovsrec_interface *iface = NULL;
    struct smap hw_intf_info, user_config;

    /* Create bridge */
    default_bridge_row = ovsrec_bridge_insert(txn);
    ovsrec_bridge_set_name(default_bridge_row, DEFAULT_BRIDGE_NAME);
    ovsrec_system_set_bridges(ovs_row, &default_bridge_row, 1);

    /*
     * For every bridge we will create a bridge port and a bridge
     * internal interface under it. The bridge internal interface
     * will be used for internal interfaces such as vlan interfaces.
     * For vlan interfaces, all vlan tagged frames for vlan interfaces
     * will be sent up to bridge interface.
     * bridge interface will distribute them to the appropriate vlan
     * interfaces which will be created on top of the bridge interface.
     */

    /* Create bridge internal interface */
    iface = ovsrec_interface_insert(txn);
    ovsrec_interface_set_name(iface, DEFAULT_BRIDGE_NAME);
    ovsrec_interface_set_type(iface, OVSREC_INTERFACE_TYPE_INTERNAL);

    smap_init(&hw_intf_info);
    smap_add(&hw_intf_info, INTERFACE_HW_INTF_INFO_MAP_BRIDGE,
             INTERFACE_HW_INTF_INFO_MAP_BRIDGE_TRUE);
    ovsrec_interface_set_hw_intf_info(iface, &hw_intf_info);
    smap_destroy(&hw_intf_info);

    /*
     * bridge interface is used internally. Essentially
     * we do not expect user to configure this interface.
     * We will set the 'admin' to up as we create it.
     */
    smap_init(&user_config);
    smap_add(&user_config, INTERFACE_USER_CONFIG_MAP_ADMIN,
             OVSREC_INTERFACE_USER_CONFIG_ADMIN_UP);

    ovsrec_interface_set_user_config(iface, &user_config);
    smap_destroy(&user_config);

    /* Create port for bridge */
    port = ovsrec_port_insert(txn);
    ovsrec_port_set_name(port, DEFAULT_BRIDGE_NAME);

    /* Add the internal interface to port */
    ovsrec_port_set_interfaces(port, &iface, 1);

    /* Add port to the bridge */
    ovsrec_bridge_set_ports(default_bridge_row, &port, 1);

}/* sysd_configure_default_bridge */

/*
 * This function is used to initialize the default VRF during system bootup.
 */
void
sysd_configure_default_vrf(struct ovsdb_idl_txn *txn,
                           struct ovsrec_system *ovs_row)
{
    struct ovsrec_vrf *default_vrf_row = NULL;
    struct smap smap_vrf_status;
    const int64_t table_id = 0;

    default_vrf_row = ovsrec_vrf_insert(txn);
    ovsrec_vrf_set_name(default_vrf_row, DEFAULT_VRF_NAME);
    ovsrec_system_set_vrfs(ovs_row, &default_vrf_row, 1);
    ovsrec_vrf_set_table_id(default_vrf_row, &table_id, sizeof(table_id));
    smap_clone(&smap_vrf_status, &default_vrf_row->status);
    smap_add_once(&smap_vrf_status, VRF_STATUS_KEY, VRF_STATUS_VALUE);

    ovsrec_vrf_set_status(default_vrf_row, &smap_vrf_status);
    smap_destroy(&smap_vrf_status);

}/* sysd_configure_default_vrf */

/*
 * Helper function to parse version_detail yaml file.
 */
static int
package_info_mapping_check_key(const char *data)
{
    int i = PKG;
    for (i = PKG; i < MAX_NUM_KEYS; i++) {
        if ((data != NULL) && (keystr_pkg_info[i] != NULL) &&
            (!strcmp(keystr_pkg_info[i], data))) {
            return i;
        }
    }
    /* Data didn't match any of the token names, hence it should be a value */
    return VALUE;
}

/*
 * Function to populate source url, type and version of each package/daemon
 * extracted from /var/lib/version_detail.yaml file to "Package_Info"
 * table in OVSDB.
 */
static void
sysd_add_package_info()
{
    FILE * fh         = NULL;
    int event_value   = 0;
    int current_state = 0;
    int record_count  = 0;
    int done          = 0;
    yaml_parser_t parser;
    yaml_event_t event;
    struct ovsrec_package_info *row  = NULL;
    struct ovsdb_idl_txn       *txn  = NULL;
    enum ovsdb_idl_txn_status  txn_status = TXN_ERROR;

    /* Initialize parser */
    if (!yaml_parser_initialize(&parser)) {
        VLOG_ERR("Failed to initialize parser\n");
        return;
    }

    /* Open /var/lib/version_detail.yaml file */
    fh = fopen(VERSION_DETAIL_FILE_PATH, "r");
    if (NULL == fh) {
        VLOG_ERR("Failed to open file %s\n",VERSION_DETAIL_FILE_PATH);
        yaml_parser_delete(&parser);
        return;
    }

    /* Set input file */
    yaml_parser_set_input_file(&parser, fh);

    /*
     * Parse version_detail file line-wise to extract package/daemon name,
     * corresponding type, version and its source-URL.
     */

    while (!done) {

        if (!yaml_parser_parse(&parser, &event)) {
            break;
        }

        if (event.type == YAML_SCALAR_EVENT) {
            event_value = package_info_mapping_check_key ((const char *)
            event.data.scalar.value);
            switch (event_value) {
                case VALUE:
                {
                    switch(current_state) {
                        case PKG:
                            if ((record_count % PKG_INFO_ENTRIES_PER_COMMIT) == 0) {
                                txn = ovsdb_idl_txn_create(idl);
                            }
                            row = ovsrec_package_info_insert(txn);
                            if (NULL == row) {
                                VLOG_ERR("Could not insert a row into DB\n");
                                /* Cleanup */
                                yaml_event_delete(&event);
                                yaml_parser_delete(&parser);
                                fclose(fh);
                                ovsdb_idl_txn_destroy(txn);
                                return;
                            }
                            ovsrec_package_info_set_name(row,
                            (const char *)event.data.scalar.value);
                            break;
                        case PV:
                            ovsrec_package_info_set_version(row,
                            (const char *)event.data.scalar.value);
                            break;
                        case SRCREV:
                            if(((const char*)event.data.scalar.value != NULL)
                            && strcmp((const char*)event.data.scalar.value,
                            "INVALID")) {
                                ovsrec_package_info_set_version(row,
                                (const char *)event.data.scalar.value);
                            }
                            break;
                        case SRC_URL:
                            ovsrec_package_info_set_src_url(row,
                            (const char *)event.data.scalar.value);
                            break;
                        case TYPE:
                            record_count++;
                            ovsrec_package_info_set_src_type(row,
                            (const char *)event.data.scalar.value);

                            /*
                             * Commit the transaction for every
                             * PKG_INFO_ENTRIES_PER_COMMIT entries
                             */
                            if ((record_count % PKG_INFO_ENTRIES_PER_COMMIT)
                                == 0) {
                                txn_status = ovsdb_idl_txn_commit_block(txn);
                                VLOG_INFO("Populating Package_Info with"
                                          "%d entries\n", record_count);
                                if (txn_status != TXN_SUCCESS) {
                                    VLOG_ERR("Commit failed to Package_Info."
                                             "rc = %u", txn_status);
                                }
                                ovsdb_idl_txn_destroy(txn);
                                txn = NULL;
                            }
                            break;
                    }
                }
                break;
                case PKG:
                    current_state = PKG;
                    break;
                case PV:
                    current_state = PV;
                    break;
                case SRCREV:
                    current_state = SRCREV;
                    break;
                case SRC_URL:
                    current_state = SRC_URL;
                    break;
                case TYPE:
                    current_state = TYPE;
                    break;
            }
        }

        done = (event.type == YAML_STREAM_END_EVENT);

        yaml_event_delete(&event);
    }

    if (txn != NULL) {
        txn_status = ovsdb_idl_txn_commit_block(txn);
        VLOG_INFO("Populating Package_Info with %d entries\n", record_count);
        if (txn_status != TXN_SUCCESS) {
            VLOG_ERR("Commit failed to Package_Info. rc = %u", txn_status);
        }
        ovsdb_idl_txn_destroy(txn);
        txn = NULL;
    }

    /* Cleanup */
    yaml_parser_delete(&parser);
    fclose(fh);

} /* sysd_add_package_info */

/*
 * Function to update the software info, e.g. software name, switch version,
 * in the OVSDB retrieved from the Release file.
 */
static void
sysd_update_sw_info(const struct ovsrec_system *cfg)
{
#define NSTR  80 /* Max length of each line of /etc/os-release. */
    struct smap smap = SMAP_INITIALIZER(&smap);
    FILE   *os_ver_fp = NULL;
    char   *line = NULL;
    char   *value;
    char   *name;
    char   version_id[NSTR];
    char   build_id[NSTR];
    char   build_str[NSTR];
    size_t line_len = 0;
    int i;

    /* Open os-release file with the os version information */
    os_ver_fp = fopen(OS_RELEASE_FILE_PATH, "r");
    if (NULL == os_ver_fp) {
        VLOG_ERR("Unable to find system OS release. File %s was not found",
                 OS_RELEASE_FILE_PATH);
        return;
    }

    /* Initialize the version_id and build_id to avoid the ops-sysd crash */
    version_id[0] = build_id[0] = '\0';

    /* Scanning file for variables */
    while (getline(&line, &line_len, os_ver_fp) != -1) {
        if (line == NULL || (value = strchr(line, '=')) == NULL) {
            /* Skip the line. */
            continue;
        }
        name = line;
        value[0] = '\0'; /* Terminate the name string. */
        ++value;
        /* Terminate the value string. */
        for (i = strlen(value) - 1; i >= 0; --i) {
            if (!isspace(value[i])) {
                break;
            }
        }
        value[++i] = '\0';

        /* Release name value.  */
        if (strcmp(name, OS_RELEASE_NAME) == 0 && value[0] != '\0') {
            smap_add(&smap, SYSTEM_SOFTWARE_INFO_OS_NAME, value);

        /* Version ID value*/
        } else if (strcmp(name, OS_RELEASE_VERSION_NAME) == 0) {
            strncpy(version_id, value, NSTR - 1);
            /* in case file_var_value is longer than NSTR - 1 */
            version_id[NSTR - 1] = '\0';

        /* Build ID value*/
        } else if (strcmp(name, OS_RELEASE_BUILD_NAME) == 0) {
            strncpy(build_id, value, NSTR - 1);
            /* in case file_var_value is longer than NSTR - 1 */
            build_id[NSTR - 1] = '\0';
        }
    }
    fclose(os_ver_fp);
    if (line != NULL) {
        /*
         * As getline(3) explains, caller of the getline() needs to
         * free the dynamically allocated memory.
         */
        free(line);
    }

    /* Update the software info column. */
    if (!smap_is_empty(&smap)) {
        ovsrec_system_set_software_info(cfg, &smap);
    }
    smap_destroy(&smap);

    /* Check if version id and build id was found*/
    if (build_id[0] != '\0' && version_id[0] != '\0') {
        /* Building the version string */
        snprintf(build_str, NSTR, "%s (Build: %s)", version_id, build_id);
        ovsrec_system_set_switch_version(cfg, build_str);
    } else {
        VLOG_ERR("%s or %s was not found on %s", OS_RELEASE_VERSION_NAME,
                 OS_RELEASE_BUILD_NAME, OS_RELEASE_FILE_PATH);
        return;
    }

} /* sysd_update_sw_info */

/*
 * Function to handle timezone updates from OVSDB.System.timezone column
 */
static void
sysd_handle_timezone_update(const struct ovsrec_system *cfg)
{
    const struct ovsdb_datum *data = NULL;
    char *ovsdb_timezone = NULL;
    char timezone_cmd[200];
    char base_path[] = "/usr/share/zoneinfo/posix/";
    int ret_val = 0;

    cfg = ovsrec_system_first(idl);

    if (cfg != NULL)
    {
        data = ovsrec_system_get_timezone(cfg, OVSDB_TYPE_STRING);
        ovsdb_timezone = data->keys->string;
        VLOG_ERR("1 sysd_handle_timezone_update : run \n");
        if (ovsdb_timezone != NULL)
        {
          memset(&timezone_cmd[0], 0, sizeof(timezone_cmd));
          strcpy(timezone_cmd, base_path);
          strcat(timezone_cmd, ovsdb_timezone);
          unlink("/etc/localtime");
          ret_val = symlink(timezone_cmd, "/etc/localtime");
          if (ret_val < 0) {
            VLOG_ERR("Unable to create symbolic link for timezone %s err code %d\n",timezone_cmd, ret_val);
            return;
          }
        }
        else
        {
            VLOG_ERR("Timezone not configured\n");
            return;
        }
    }
    else
    {
        VLOG_ERR("Error retrieving Timezone info\n");
        return;
    }
} /* sysd_handle_timezone_update */

void
sysd_initial_configure(struct ovsdb_idl_txn *txn)
{
    int     i = 0;
    char    mac_addr[32];
    char    *tmp_p;
    struct ovsrec_daemon **ovs_daemon_l = NULL;
    struct ovsrec_subsystem **ovs_subsys_l = NULL;
    struct ovsrec_system *sys = NULL;
    struct smap smap = SMAP_INITIALIZER(&smap);

    /* Add System row */
    sys = ovsrec_system_insert(txn);

    /* Add the interface name to ovsdb */
    smap_add(&smap, SYSTEM_MGMT_INTF_MAP_NAME, mgmt_intf->name);

    ovsrec_system_set_mgmt_intf(sys, &smap);
    smap_destroy(&smap);

    /* Add default bridge and VRF rows */
    sysd_configure_default_bridge(txn, sys);
    sysd_configure_default_vrf(txn, sys);

    /* Assign system wide mgmt i/f MAC address
     * Set System:management_mac
    */

    /* OPS_TODO: Need to update for multiple subsystem
     * for now, assume that subsystem[0] is the base subsystem and use
     * the mgmt MAC for the base subsystem as the system wide mgmt MAC.
    */

    memset(mac_addr, 0, sizeof(mac_addr));
    tmp_p = ops_ether_ulong_long_to_string(mac_addr, subsystems[0]->mgmt_mac_addr);
    ovsrec_system_set_management_mac(sys, tmp_p);

    /* Assign general use MAC */
    /* OPS_TODO: Using subsystem[0] for now */
    memset(mac_addr, 0, sizeof(mac_addr));
    tmp_p = ops_ether_ulong_long_to_string(mac_addr, subsystems[0]->system_mac_addr);
    ovsrec_system_set_system_mac(sys, tmp_p);

    /* Add the subsystem info to OVSD */
    ovs_subsys_l = SYSD_OVS_PTR_CALLOC(ovsrec_subsystem *, num_subsystems);
    if (ovs_subsys_l == NULL) {
        VLOG_ERR("Failed to allocate memory for OVS subsystem.");
        log_event("SYS_ALLOCATE_MEMORY_FAILURE", EV_KV("value",
            "%s", "OVS subsystem"));
        return;
    }

    for (i = 0; i < num_subsystems; i++) {
        ovs_subsys_l[i] = sysd_initial_subsystem_add(txn, subsystems[i]);
    }

    ovsrec_system_set_subsystems(sys, ovs_subsys_l, num_subsystems);

    /* Add the daemon info to the daemon table */
    ovs_daemon_l = SYSD_OVS_PTR_CALLOC(ovsrec_daemon *, num_daemons);
    if (ovs_daemon_l == NULL) {
        VLOG_ERR("Failed to allocate memory for OVS daemon table.");
        log_event("SYS_ALLOCATE_MEMORY_FAILURE", EV_KV("value",
            "%s", "OVS daemon table"));
        return;
    }

    if (num_daemons > 0) {
        for (i = 0; i < num_daemons; i++) {
            ovs_daemon_l[i] = sysd_initial_daemon_add(txn, daemons[i]);
        }

        ovsrec_system_set_daemons(sys, ovs_daemon_l, num_daemons);
    }
    free(ovs_daemon_l);

    /*
     * Update the software info, including the switch version,
     * for the new config
     */
    sysd_update_sw_info(sys);
    ovsrec_system_set_timezone(sys, DFLT_TIMEZONE);

    /* QoS init */
    qos_init_trust(txn, sys);
    qos_init_dscp_map(txn, sys);
    qos_init_cos_map(txn, sys);
    qos_init_queue_profile(txn, sys);
    qos_init_schedule_profile(txn, sys);
    /* ACL init */
    acl_init_limits(txn, sys);
} /* sysd_initial_configure */

static void
sysd_set_hw_done(void)
{
    struct ovsdb_idl_txn *txn = NULL;
    const struct ovsrec_system *sys = NULL;
    enum ovsdb_idl_txn_status txn_status = TXN_ERROR;
    char hostname[128];
    int ret;

    ret = gethostname(hostname, sizeof(hostname));
    if(ret < 0)
        VLOG_ERR("hostname:%s ret errno %s", hostname, strerror(errno));

    txn = ovsdb_idl_txn_create(idl);

    OVSREC_SYSTEM_FOR_EACH(sys, idl) {
        ovsrec_system_set_cur_hw(sys, (int64_t) 1);
        VLOG_INFO("%s system cur_hw after %d", hostname, (int)(sys->cur_hw));
        ovsrec_system_set_next_hw(sys, (int64_t) 1);
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
    char hostname[128];
    int ret = 0;

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
     *      System:{cur_hw,next_hw} = 1.
     *
     * The configuration daemon waits for sysd to set System:cur_hw=1
     * before it tries to push anything into the db, to ensure that all h/w
     * processing is done before any user configuration is pushed.
    */

    ret = gethostname(hostname, sizeof(hostname));
    if(ret < 0)
        VLOG_ERR("hostname:%s ret errno:%s", hostname, strerror(errno));

    VLOG_INFO("hostname:%s Number of Daemons running %d", hostname,
              num_hw_daemons);

    if (num_hw_daemons <= 0) {
        sysd_set_hw_done();
        return;
    }

    /* See if all h/w daemons have set cur_hw > 0 */
    for (i = 0; i < num_daemons; i++) {
        if (daemons[i]->is_hw_handler) {
            OVSREC_DAEMON_FOR_EACH(db_daemon, idl) {
                VLOG_INFO("%s db_daemon:'%s' daemons_manifest:'%s'"
                           " db_daemon cur_hw: %d db_daemon is_hw_handler: %d",
                           hostname, db_daemon->name, daemons[i]->name,
                           (int)(db_daemon->cur_hw),
                           (int)(db_daemon->is_hw_handler));
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
    if (not_set || (num_found == 0))
        return;
    else
        VLOG_INFO("%s num_found count %d", hostname, num_found);

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
    const struct ovsrec_system    *cfg = NULL;
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

        cfg = ovsrec_system_first(idl);

        if (cfg == NULL) {
            txn = ovsdb_idl_txn_create(idl);

            sysd_initial_configure(txn);

            txn_status = ovsdb_idl_txn_commit_block(txn);
            if (txn_status != TXN_SUCCESS) {
                VLOG_ERR("Failed to commit the transaction. rc = %s", ovsdb_idl_txn_status_to_string(txn_status));
            }
            ovsdb_idl_txn_destroy(txn);
        } else {
            /* Update the software information. */
            sysd_update_sw_info(cfg);

            if (!hw_init_done_set) {
                sysd_chk_if_hw_daemons_done();
            }
        }

        /* Populate source url and version of packages/daemon present in image */
        if (ovsrec_package_info_first(idl) == NULL) {
            sysd_add_package_info();
        }
        sysd_handle_timezone_update(cfg);
    }

    /* Notify parent of startup completion. */
    daemonize_complete();

} /* sysd_run */

/*
 * Function       : sysd_dump
 * Responsibility : populates buffer for unixctl reply
 * Parameters     : buffer, buffer length
 * Returns        : void
 */

void
sysd_dump(char* buf, int buflen)
{
    char tmp_buf[100];
    int i = 0;

    /* Loop through all daemons */

    strcpy(buf, "=============== Daemon Info =========================\n");
    strncat(buf, "Name\t\t\tis_hw_handler\t\t\tcur_hw\n", REM_BUF_LEN);

    while((num_hw_daemons - 1) >= i) {
        strncat(buf, daemons[i]->name, REM_BUF_LEN);
        strncat(buf, "\t\t\t", REM_BUF_LEN);
        sprintf(tmp_buf, "%d", daemons[i]->is_hw_handler);
        strncat(buf, tmp_buf, REM_BUF_LEN);
        strncat(buf, "\t\t\t", REM_BUF_LEN);
        strcpy(tmp_buf, "\0");
        sprintf(tmp_buf, "%" PRIi64, daemons[i]->cur_hw);
        strncat(buf, tmp_buf, REM_BUF_LEN);
        strncat(buf, "\n", REM_BUF_LEN);
        i++;
    }

    /* mgmt_intf */
    strncat(buf, "=============== Mgmt_intf Info ==========================\n",
            REM_BUF_LEN);
    strncat(buf, mgmt_intf->name, REM_BUF_LEN);
    strncat(buf, "\n", REM_BUF_LEN);
}

void
sysd_wait(void)
{
    ovsdb_idl_wait(idl);

} /* sysd_wait */
/** @} end of group sysd */
