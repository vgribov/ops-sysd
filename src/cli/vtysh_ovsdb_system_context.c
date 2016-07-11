/* System CLI commands
 *
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 * Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * File: vtysh_ovsdb_system_context.c
 *
 * Purpose:  To add system CLI configuration and display commands.
 */

#include "vtysh/vty.h"
#include "vtysh/vector.h"
#include "vswitch-idl.h"
#include "openswitch-idl.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "vtysh/utils/system_vtysh_utils.h"
#include "vtysh_ovsdb_system_context.h"

/*
 * Function        : vtysh_config_context_system_callback
 * Resposibility   : Handler to provide timezone with show running-config
 */
vtysh_ret_val
vtysh_config_context_system_callback(void *p_private)
{
    vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
    const struct ovsdb_datum *data = NULL;
    char *ovsdb_timezone = NULL;
    char timezone[200];
    const struct ovsrec_system *vswrow;
    int i = 0;
    vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_DBG,
                              "vtysh_config_context_timezone_callback entered");
    vswrow = ovsrec_system_first(p_msg->idl);

    if(vswrow)
    {
        data = ovsrec_system_get_timezone(vswrow, OVSDB_TYPE_STRING);
        ovsdb_timezone = data->keys->string;
        if (ovsdb_timezone != NULL)
        {
            strcpy(timezone, ovsdb_timezone);
            for (i=0;i<strlen(timezone);i++) {
              timezone[i] = tolower(timezone[i]);
            }
            vtysh_ovsdb_cli_print(p_msg, "timezone set %s", timezone);
        }
    }

    return e_vtysh_ok;
}
