/****************************************************************************
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
 ***************************************************************************/

#include <config.h>

#include "acl_init.h"

#include <ops-utils.h>
#include <stdlib.h>
#include <sys/types.h>

#include "config-yaml.h"
#include "sysd_cfg_yaml.h"
#include "smap.h"
#include "util.h"
#include "vswitch-idl.h"
#include "openvswitch/vlog.h"

#define ACL_LIMIT_BUFFER_SIZE 16

/* Key strings defined in vswitch.xml that could/should be generated with IDL
   e.g. OVSREC_SYSTEM_OTHER_INFO_KEY_MAX_ACLS */
#define ACL_LIMIT_KEY_MAX_ACLS "max_acls"
#define ACL_LIMIT_KEY_MAX_ACES "max_aces"
#define ACL_LIMIT_KEY_MAX_ACES_PER_ACL "max_aces_per_acl"

VLOG_DEFINE_THIS_MODULE(acl_init_limits);

/**
 * Initializes acl max acls and max aces for the given txn and system_row.
 */
void
acl_init_limits(struct ovsdb_idl_txn *txn,
                struct ovsrec_system *system_row)
{
    YamlAclInfo *acl_info;
    struct smap smap;
    char max_acls_str[ACL_LIMIT_BUFFER_SIZE];
    char max_aces_str[ACL_LIMIT_BUFFER_SIZE];
    char max_aces_per_acl_str[ACL_LIMIT_BUFFER_SIZE];

    acl_info = sysd_cfg_yaml_get_acl_info();

    /* smap_replace expects char values */
    snprintf(max_acls_str, ACL_LIMIT_BUFFER_SIZE, "%d", acl_info->max_acls);
    snprintf(max_aces_str, ACL_LIMIT_BUFFER_SIZE, "%d", acl_info->max_aces);
    snprintf(max_aces_per_acl_str, ACL_LIMIT_BUFFER_SIZE, "%d", acl_info->max_aces_per_acl);

    /* Store acl limitations in other_info column */
    smap_clone(&smap, &system_row->other_info);
    smap_replace(&smap, ACL_LIMIT_KEY_MAX_ACLS, max_acls_str);
    smap_replace(&smap, ACL_LIMIT_KEY_MAX_ACES, max_aces_str);
    smap_replace(&smap, ACL_LIMIT_KEY_MAX_ACES_PER_ACL, max_aces_per_acl_str);
    ovsrec_system_set_other_info(system_row, &smap);
    smap_destroy(&smap);
    return;
}
