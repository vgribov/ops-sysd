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
 * File: system_vty.c
 *
 * Purpose:  To add system CLI configuration and display commands.
 */

#include "vtysh/command.h"
#include "vtysh/vtysh.h"
#include "vtysh/vtysh_user.h"
#include "system_vty.h"
#include "vswitch-idl.h"
#include "ovsdb-idl.h"
#include "smap.h"
#include "vtysh/memory.h"
#include "openvswitch/vlog.h"
#include "openswitch-idl.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "vtysh/utils/system_vtysh_utils.h"
#include <dirent.h>
#include "vtysh_ovsdb_system_context.h"

VLOG_DEFINE_THIS_MODULE(vtysh_system_cli);

extern struct ovsdb_idl *idl;
/*
 * Function        : compare_fan
 * Resposibility     : Fan sort function for qsort
 * Parameters
 *  a   : Pointer to 1st element in the array
 *  b   : Pointer to next element in the array
 * Return      : comparative difference between names.
 */
static inline int
compare_fan (const void* a,const void* b)
{
    struct ovsrec_fan* s1 = (struct ovsrec_fan*)a;
    struct ovsrec_fan* s2 = (struct ovsrec_fan*)b;

    return (strcmp(s1->name,s2->name));
}

/*
 * Function        : compare_psu
 * Resposibility    : Power Supply sort function for qsort
 * Parameters
 *   a   : Pointer to 1st element in the array
 *   b   : Pointer to next element in the array
 * Return      : comparative difference between names.
 */
static inline int
compare_psu(const void* a,const void* b)
{
    struct ovsrec_power_supply* s1 = (struct ovsrec_power_supply*)a;
    struct ovsrec_power_supply* s2 = (struct ovsrec_power_supply*)b;

    return (strcmp(s1->name,s2->name));
}

/*
 * Function        : format_psu_string
 * Resposibility     : Change status string in OVSDB to more
 *        readable string
 * Parameters
 *      status  : Pointer to status string
 * Return      : Pointer to formatted status string
 */
static const char*
format_psu_string (char* status)
{
    if (!status)
        return NULL;

    if (0 == strcmp(status,OVSREC_POWER_SUPPLY_STATUS_FAULT_ABSENT))
        return POWER_SUPPLY_FAULT_ABSENT;
    else if (0 == strcmp(status,OVSREC_POWER_SUPPLY_STATUS_FAULT_INPUT))
        return POWER_SUPPLY_FAULT_INPUT;
    else if (0 == strcmp(status,OVSREC_POWER_SUPPLY_STATUS_FAULT_OUTPUT))
        return POWER_SUPPLY_FAULT_OUTPUT;
    else if (0 == strcmp(status,OVSREC_POWER_SUPPLY_STATUS_UNKNOWN))
        return POWER_SUPPLY_UNKNOWN;
    else if (0 == strcmp(status,OVSREC_POWER_SUPPLY_STATUS_OK))
        return POWER_SUPPLY_OK;

    return status;
}

/*
 * Function        : format_sys_output
 * Resposibility     : Format and Print output for system info
 * Parameters
 *      vty : Pointer to vty structure
 *  pSys    : Pointer to ovsrec_subsystem structure
 *  pVswitch: Pointer to ovsrec_system structure
 */
static void
format_sys_output (struct vty* vty,
                const struct ovsrec_subsystem* pSys,
                const struct ovsrec_system* pVswitch)
{
    const char* buf = NULL;
    vty_out(vty, "%-20s%s%-30s%s", "OpenSwitch Version", ": ",
                 (pVswitch->switch_version) ? pVswitch->switch_version : " ",
                 VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"Product Name");
    (buf) ? vty_out(vty,"%-20s%s%-30s%s%s",
            "Product Name",": ",buf,VTY_NEWLINE,VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-30s%s%s",
                "Product Name",": "," ",VTY_NEWLINE,VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"vendor");
    (buf) ? vty_out(vty,"%-20s%s%-30s%s","Vendor",": ", buf, VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-30s%s","Vendor",": "," ", VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"platform_name");
    (buf) ? vty_out(vty,"%-20s%s%-30s%s","Platform",": ", buf, VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-30s%s","Platform",": "," ", VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"manufacturer");
    (buf) ? vty_out(vty,"%-20s%s%-20s%s","Manufacturer",": ",buf,VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-20s%s","Manufacturer",": "," ", VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"manufacture_date");
    (buf) ? vty_out(vty,"%-20s%s%-20s%s%s",
            "Manufacturer Date",": ", buf, VTY_NEWLINE, VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-20s%s%s",
                "Manufacturer Date",": "," ", VTY_NEWLINE, VTY_NEWLINE);


    buf = smap_get (&pSys->other_info,"serial_number");
    (buf) ? vty_out(vty,"%-20s%s%-20s","Serial Number",": ", buf):\
        vty_out(vty,"%-20s%s%-20s","Serial Number",": "," ");

    buf = smap_get (&pSys->other_info,"label_revision");
    (buf) ? vty_out(vty,"%-20s%s%-10s%s%s",
            "Label Revision",": ", buf, VTY_NEWLINE,VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-10s%s%s",
                "Label Revision",": "," ", VTY_NEWLINE,VTY_NEWLINE);


    buf = smap_get (&pSys->other_info,"onie_version");
    (buf) ? vty_out(vty,"%-20s%s%-20s","ONIE Version",": ", buf):\
        vty_out(vty,"%-20s%s%-20s","ONIE Version",": "," ");

    buf = smap_get (&pSys->other_info,"diag_version");
    (buf) ? vty_out(vty,"%-20s%s%-10s%s",
            "DIAG Version",": ", buf, VTY_NEWLINE):\
        vty_out(vty,"%-20s%s%-10s%s","DIAG Version",": "," ", VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"base_mac_address");
    (buf) ? vty_out(vty, "%-20s%s%-20s","Base MAC Address",": ", buf):\
        vty_out(vty,"%-20s%s%-20s","Base MAC Address",": "," ");

    buf = smap_get (&pSys->other_info,"number_of_macs");
    (buf) ? vty_out(vty, "%-20s%s%-5s%s",
            "Number of MACs",": ", buf, VTY_NEWLINE):\
        vty_out(vty, "%-20s%s%-5s%s","Number of MACs",": "," ", VTY_NEWLINE);

    buf = smap_get (&pSys->other_info,"interface_count");
    (buf) ? vty_out(vty, "%-20s%s%-20s","Interface Count",": ", buf):\
        vty_out(vty, "%-20s%s%-20s","Interface Count",": "," ");

    buf = smap_get (&pSys->other_info,"max_interface_speed");
    (buf) ? vty_out(vty, "%-20s%s%-6sMbps%s",
            "Max Interface Speed",": ", buf, VTY_NEWLINE):\
        vty_out(vty, "%-20s%s%-6sMbps%s",
                "Max Interface Speed",": "," ", VTY_NEWLINE);
}


/*
 * Function        : cli_system_get_all
 * Resposibility     : Get System overview information from OVSDB
 * Return      : 0 on success 1 otherwise
 */
int
cli_system_get_all()
{
    const struct ovsrec_subsystem* pSys = NULL;
    const struct ovsrec_system* pVswitch = NULL;
    const struct ovsrec_fan* pFan = NULL;
    struct ovsrec_fan* pFanSort = NULL;
    const struct ovsrec_led* pLed = NULL;
    const struct ovsrec_power_supply* pPSU = NULL;
    struct ovsrec_power_supply* pPSUsort = NULL;
    const struct ovsrec_temp_sensor* pTempSen = NULL;
    int n = 0, i = 0;

    pSys = ovsrec_subsystem_first(idl);
    pVswitch = ovsrec_system_first(idl);

    if (pSys && pVswitch) {
        format_sys_output(vty, pSys,pVswitch);
    }
    else {
        VLOG_ERR("Unable to retrieve data\n");
    }


    vty_out(vty,"%sFan details:%s%s",VTY_NEWLINE,VTY_NEWLINE, VTY_NEWLINE);
    vty_out(vty,"%-15s%-10s%-10s%s","Name","Speed","Status",VTY_NEWLINE);
    vty_out(vty,"%s%s","--------------------------------",VTY_NEWLINE);
    n = pSys->n_fans;
    if (0 != n)
    {
        pFanSort = (struct ovsrec_fan*)calloc (n,sizeof(struct ovsrec_fan));

        OVSREC_FAN_FOR_EACH (pFan,idl)
        {
            memcpy (pFanSort+i,pFan,sizeof(struct ovsrec_fan));
            i++;
        }

        qsort((void*)pFanSort,n,sizeof(struct ovsrec_fan),compare_fan);

        for (i = 0; i < n ; i++)
        {
            vty_out(vty,"%-15s",(pFanSort+i)->name);
            vty_out(vty,"%-10s",(pFanSort+i)->speed);
            vty_out(vty,"%-10s",(pFanSort+i)->status);
            vty_out(vty,"%s",VTY_NEWLINE);
        }
    }

    if (pFanSort)
    {
        free(pFanSort);
        pFanSort = NULL;
    }

    vty_out(vty,"%sLED details:%s%s",VTY_NEWLINE,VTY_NEWLINE, VTY_NEWLINE);
    vty_out(vty,"%-10s%-10s%-8s%s","Name","State","Status",VTY_NEWLINE);
    vty_out(vty,"%s%s","-------------------------",VTY_NEWLINE);

    n = pSys->n_leds;
    if (0 != n)
    {

        OVSREC_LED_FOR_EACH (pLed,idl)
        {
            vty_out(vty,"%-10s",pLed->id);
            vty_out(vty,"%-10s",pLed->state);
            vty_out(vty,"%-8s",pLed->status);
            vty_out(vty,"%s",VTY_NEWLINE);
        }
    }

    vty_out(vty,"%sPower supply details:%s%s",VTY_NEWLINE,VTY_NEWLINE,
            VTY_NEWLINE);
    vty_out(vty,"%-10s%-10s%s","Name","Status",VTY_NEWLINE);
    vty_out(vty,"%s%s","-----------------------",VTY_NEWLINE);
    n = pSys->n_power_supplies;

    if (n > 0)
    {
        pPSUsort = (struct ovsrec_power_supply*)calloc(n,
					sizeof(struct ovsrec_power_supply));

        i = 0;
        OVSREC_POWER_SUPPLY_FOR_EACH (pPSU,idl)
        {
            if (pPSU)
            {
                memcpy(pPSUsort+i,pPSU,sizeof(struct ovsrec_power_supply));
                i++;
            }
	}

        qsort((void*)pPSUsort,n,sizeof(struct ovsrec_power_supply),compare_psu);

        for (i = 0; i < n ; i++)
        {
            vty_out(vty,"%-15s",(pPSUsort+i)->name);
            vty_out(vty,"%-10s",format_psu_string((pPSUsort+i)->status));
            vty_out(vty,"%s",VTY_NEWLINE);
        }

        if(pPSUsort)
	{
            free(pPSUsort);
            pPSUsort = NULL;
	}
    }

    vty_out(vty,"%sTemperature Sensors:%s%s",VTY_NEWLINE,VTY_NEWLINE,
            VTY_NEWLINE);
    n = pSys->n_temp_sensors;
    if ( 0 !=  n)
    {

        vty_out(vty,"%-50s%-10s%-18s%s","Location","Name",
                "Reading(celsius)",VTY_NEWLINE);
        vty_out(vty,"%s%s",
                "---------------------------------------------------------------------------",VTY_NEWLINE);
        OVSREC_TEMP_SENSOR_FOR_EACH (pTempSen,idl)
        {
            vty_out(vty,"%-50s",pTempSen->location);
            vty_out(vty,"%-10s",pTempSen->name);
            vty_out(vty,"%3.2f",(double)((pTempSen->temperature)/1000));
            vty_out(vty,"%s",VTY_NEWLINE);
        }
    }
    else
    {
        vty_out(vty,"%-10s%-10s%-18s%s","Location","Name",
                "Reading(celsius)",VTY_NEWLINE);
        vty_out(vty,"%s%s","------------------------------------",VTY_NEWLINE);
    }

    return CMD_SUCCESS;
}


DEFUN (cli_platform_show_system,
        cli_platform_show_system_cmd,
        "show system",
        SHOW_STR
        SYS_STR)
{
    return cli_system_get_all();
}

DEFUN_NOLOCK ( vtysh_show_system_clock,
        vtysh_show_system_clock_cmd,
        "show system clock",
        SHOW_STR
        SYS_STR
        CLOCK_STR
      )
{
    execute_command("date", 0, (const char **) NULL);
    return CMD_SUCCESS;
}
DEFUN_NOLOCK ( vtysh_show_date,
        vtysh_show_date_cmd,
        "show date",
        SHOW_STR
        DATE_STR
      )
{
    execute_command("date", 0, (const char **) NULL);
    return CMD_SUCCESS;
}

/*
 * Function        : vtysh_show_system_timezone
 * Resposibility   : Displays Timezone information configured on the system
 * Return          : Returns CMD_SUCCESS on successful completion
 */

DEFUN_NOLOCK ( vtysh_show_system_timezone,
        vtysh_show_system_timezone_cmd,
        "show system timezone",
        SHOW_STR
        SYS_STR
        TIMEZONE_STR
      )
{
    char temp_file_path[]= "/tmp/timezone_info";
    char output[VTY_BUFSIZ];
    FILE *tmp_fp = NULL;
    const struct ovsrec_system *ovs= NULL;
    struct ovsdb_idl_txn* status_txn = NULL;
    enum ovsdb_idl_txn_status status = TXN_ERROR;
    const struct ovsdb_datum *data = NULL;
    char *ovsdb_timezone = NULL;

    ovs = ovsrec_system_first(idl);
    if (ovs) {
        status_txn = cli_do_config_start();
        if(status_txn == NULL) {
            cli_do_config_abort(status_txn);
            VLOG_ERR("Couldn't create the OVSDB transaction.");
        } else {
            data = ovsrec_system_get_timezone(ovs, OVSDB_TYPE_STRING);
            ovsdb_timezone = data->keys->string;
            if (ovsdb_timezone != NULL)
            {
               vty_out(vty, "System is configured for timezone : %s\n",ovsdb_timezone);
            }

            status = cli_do_config_finish(status_txn);
        }
        if(!(status == TXN_SUCCESS || status == TXN_UNCHANGED))
            VLOG_ERR("Committing transaction to DB failed.");
    } else {
        VLOG_ERR("Unable to retrieve any system table rows");
    }


    remove(temp_file_path);
    tmp_fp = popen("timedatectl status >> /tmp/timezone_info","w");
    pclose(tmp_fp);
    tmp_fp = fopen(temp_file_path, "r");
    int line_count = 0;
    if (tmp_fp)
    {
        while ( fgets( output, VTY_BUFSIZ, tmp_fp ) != NULL && line_count < 6)
            line_count++;
        while ( fgets( output, VTY_BUFSIZ, tmp_fp ) != NULL)
            vty_out (vty, "%s", output);
    }
    else
    {
        VLOG_ERR("Failed to get information");
        return CMD_ERR_NO_MATCH;
    }
    fclose(tmp_fp);
    return CMD_SUCCESS;
}

/*
 * Function        : translate_cmd_to_filepath
 * Resposibility   : Maps user provided timezone info to full file path
 * Return          : Returns -1 on error and zone_index on successful completion
 */
int translate_cmd_to_filepath(char *timezone_cmd, char *timezone_info) {
  int i = 0;
  for (i=0;i<zone_count;i++) {
    if (strcmp(list_of_zones[i], timezone_cmd) == 0) {
      strcpy(timezone_info, list_of_zones[i]);
      return i;
    }
  }
  timezone_info[0] = 0;
  return -1;
}

/*
 * Function        : cli_set_timezone
 * Resposibility   : Helper function to set/unset the user configured timezone into OVSDB
 * Return          : Returns CMD_ERR_NO_MATCH on error and CMD_SUCCESS on successful completion
 */
int cli_set_timezone(char *timezone_info, int no_flags) {
    int ret_val = -1;
    char timezone_cmd[500];
    char timezone[MAX_TIMEZONE_NAME_SIZE];
    const struct ovsrec_system *ovs= NULL;
    struct ovsdb_idl_txn* status_txn = NULL;
    enum ovsdb_idl_txn_status status = TXN_ERROR;
    const struct ovsdb_datum *data = NULL;
    char *ovsdb_timezone = NULL;
    char timezone_from_ovsdb[MAX_TIMEZONE_NAME_SIZE];
    int i=0;

    memset(timezone, 0, sizeof(timezone));
    memset(timezone_cmd, 0, sizeof(timezone_cmd));
    strcpy(timezone_cmd, DEFAULT_TIMEZONE);
    memset(timezone_from_ovsdb, 0, sizeof(timezone_from_ovsdb));
    ovs = ovsrec_system_first(idl);
    if (ovs) {
        status_txn = cli_do_config_start();
        if(status_txn == NULL) {
            cli_do_config_abort(status_txn);
            VLOG_ERR("Couldn't create the OVSDB transaction.");
        } else {
            data = ovsrec_system_get_timezone(ovs, OVSDB_TYPE_STRING);
            ovsdb_timezone = data->keys->string;
            if (ovsdb_timezone != NULL)
            {
                strcpy(timezone_from_ovsdb, ovsdb_timezone);
                for (i=0;i<strlen(timezone_from_ovsdb);i++) {
                  timezone_from_ovsdb[i] = tolower(timezone_from_ovsdb[i]);
                }
                if (no_flags && (strcmp(timezone_from_ovsdb, timezone_info) == 0)) {
                  strcpy(timezone_cmd, DEFAULT_TIMEZONE);
                } else if (!no_flags) {
                  strcpy(timezone_cmd, timezone_info);
                } else {
                  cli_do_config_abort(status_txn);
                  vty_out(vty, "%s %s%s", "Timezone configured is", timezone_from_ovsdb, VTY_NEWLINE);
                  return CMD_ERR_NO_MATCH;
                }
            }

            ret_val = translate_cmd_to_filepath(timezone_cmd,timezone);
            if (ret_val < 0) {
              vty_out(vty, "%s%s", "Invalid timezone specified, please use a valid timezone", VTY_NEWLINE);
              cli_do_config_abort(status_txn);
              return CMD_ERR_NO_MATCH;
            }
            strcpy(timezone_cmd, "/usr/share/zoneinfo/posix");
            strcat(timezone_cmd, list_of_zones_caps[ret_val]);

            ovsrec_system_set_timezone(ovs, list_of_zones_caps[ret_val]);
            status = cli_do_config_finish(status_txn);
        }
        if(!(status == TXN_SUCCESS || status == TXN_UNCHANGED))
            VLOG_ERR("Committing transaction to DB failed.");
    } else {
        VLOG_ERR("unable to retrieve any system table rows");
    }
    return CMD_SUCCESS;
}

/*
 * Function        : cli_platform_timezone_set
 * Resposibility   : Sets the user configured timezone into OVSDB
 * Return          : Returns CMD_ERR_NO_MATCH on error and CMD_SUCCESS on successful completion
 */
DEFUN (cli_platform_timezone_set,
        cli_platform_timezone_set_cmd,
       "timezone set ",
       TIMEZONE_STR
       TIMEZONE_SET_STR)
{
  return cli_set_timezone((char *) argv[0], false);
}

/*
 * Function        : cli_platform_timezone_set_no_form
 * Resposibility   : Unsets the user configured timezone into OVSDB
 * Return          : Returns CMD_ERR_NO_MATCH on error and CMD_SUCCESS on successful completion
 */
DEFUN (cli_platform_timezone_set_no_form,
       cli_platform_timezone_set_cmd_no_form,
       "no timezone set ",
       NO_STR
       TIMEZONE_STR
       TIMEZONE_SET_STR)
{
  return cli_set_timezone((char *) argv[0], true);
}

/*******************************************************************
 * @func        : system_ovsdb_init
 * @detail      : Add system related table & columns to ops-cli
 *                idl cache
 *******************************************************************/
static void
system_ovsdb_init()
{
    /* Add Platform Related Tables. */
    ovsdb_idl_add_table(idl, &ovsrec_table_fan);
    ovsdb_idl_add_table(idl, &ovsrec_table_led);
    ovsdb_idl_add_table(idl, &ovsrec_table_system);
    ovsdb_idl_add_table(idl, &ovsrec_table_subsystem);

    /* Add Columns for System Related Tables. */
    ovsdb_idl_add_column(idl, &ovsrec_system_col_timezone);

    /* LED. */
    ovsdb_idl_add_column(idl, &ovsrec_led_col_id);
    ovsdb_idl_add_column(idl, &ovsrec_led_col_state);
    ovsdb_idl_add_column(idl, &ovsrec_led_col_status);
    ovsdb_idl_add_column(idl, &ovsrec_led_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_led_col_external_ids);

    /* Subsystem .*/
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_interfaces);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_leds);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_fans);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_asset_tag_number);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_type);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_hw_desc_dir);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_other_info);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_external_ids);

    /* Fan. */
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_status);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_direction);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_rpm);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_hw_config);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_external_ids);
    ovsdb_idl_add_column(idl, &ovsrec_fan_col_speed);

}

/*
 * Function        : system_install_timezone_set_command
 * Resposibility   : Installs "timezone set TIMEZONE" and "no timezone set TIMEZONE" command
 * Return          : Returns -1 on error and 0 on successful completion
 */
int system_install_timezone_set_command()
{
    char *help = NULL, *help_no_form = NULL;
    char *cmd = NULL, *cmd_no_form = NULL;

    cmd  = (char*)calloc(MAX_TIMEZONES, MAX_TIMEZONE_NAME_SIZE);
    help = (char*)calloc(MAX_TIMEZONES, MAX_TIMEZONE_HELP_SIZE);
    cmd_no_form  = (char*)calloc(MAX_TIMEZONES, MAX_TIMEZONE_NAME_SIZE);
    help_no_form = (char*)calloc(MAX_TIMEZONES, MAX_TIMEZONE_HELP_SIZE);

    if(!cmd && !help && !cmd_no_form && !help_no_form) {
        VLOG_ERR("Memory allocation failure");
        return -1;
    }

    strncpy(cmd, "timezone set (", (MAX_TIMEZONE_CMD_SIZE-1));
    strncpy(cmd_no_form, "no ", (MAX_TIMEZONE_CMD_SIZE-1));
    strncpy(help, cli_platform_timezone_set_cmd.doc,
       (MAX_TIMEZONES_HELP_SIZE-1));
    strncpy(help_no_form, cli_platform_timezone_set_cmd_no_form.doc,
         (MAX_TIMEZONES_HELP_SIZE-1));

    find_posix_timezones_and_add_to_list("/usr/share/zoneinfo/posix", 0, cmd, help);

    strncat(cmd, ")", (MAX_TIMEZONE_CMD_SIZE - strlen(cmd)));
    strncat(cmd_no_form, cmd, strlen(cmd));
    strncpy(help_no_form, help, strlen(help));

    cli_platform_timezone_set_cmd.string = cmd;
    cli_platform_timezone_set_cmd.doc = help;

    cli_platform_timezone_set_cmd_no_form.string = cmd_no_form;
    cli_platform_timezone_set_cmd_no_form.doc = help_no_form;

    //Installing element with CONFIG_NODE
    install_element (CONFIG_NODE, &cli_platform_timezone_set_cmd);
    install_element (CONFIG_NODE, &cli_platform_timezone_set_cmd_no_form);
    return 0;
}

/*
 * Function        : populate_zone_db
 * Resposibility   : Reads the posix timezones and populates info within the zone db
 *                   list_of_zones and list_of_zones_caps
 * Return          : Returns -1 on error and 0 on successful completion
 */
void populate_zone_db(char *zone, char *zone_caps)
{
  strcpy(&list_of_zones[zone_count][0], zone);
  strcpy(&list_of_zones_caps[zone_count][0], zone_caps);
  zone_count++;
}

/*
 * Function        : add_info_to_cmd_options
 * Resposibility   : Helper function to add zone with the cli command and help strings
 */
void add_info_to_cmd_options(char *timezone, char *cmd, char *help)
{
    char zone_caps[MAX_TIMEZONE_NAME_SIZE];
    int i = 0;
    memset(&zone_caps[0], 0, sizeof(zone_caps));

    strcat(help, timezone);
    strcat(help, " Zone \n");
    strcpy(zone_caps, timezone);
    for (i=0;i<strlen(timezone);i++) {
      timezone[i] = tolower(timezone[i]);
    }
    strcat(cmd, timezone);
    strcat(cmd, " | ");
    populate_zone_db(timezone, zone_caps);
}

/*
 * Function        : remove_base_path
 * Resposibility   : Removes base_path from the input string full_path
 * Parameters      :
 *  full_path - full path to timezone file
 *  base_path - top level timezone diretory
 * Return          : Returns modified full_path variable
 */
void remove_base_path(char *full_path,const char *base_path)
{
  while( (full_path=strstr(full_path,base_path)) && (full_path!=NULL) )
    memmove(full_path,full_path+strlen(base_path),1+strlen(full_path+strlen(base_path)));
}

/*
 * Function        : find_posix_timezones_and_add_to_list
 * Resposibility   : Reads the posix timezones and populates info within the zone db
 *                   list_of_zones and list_of_zones_caps
 */
void find_posix_timezones_and_add_to_list(const char *name, int level, char *cmd, char *help)
{
  DIR *dir;
  struct dirent *entry;
  char local_dir[100];
  char path[1024];
  int len;
  if (!(dir = opendir(name)))
      return;
  if (!(entry = readdir(dir)))
      return;

  do {
      if (entry->d_type == DT_DIR) {
          len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
          path[len] = 0;
          if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
              continue;
          find_posix_timezones_and_add_to_list(path, level + 1, cmd, help);
      }
      else {
          int ilen = snprintf(local_dir, sizeof(local_dir)-1, "%s/%s", name, entry->d_name);
          local_dir[ilen] = 0;
          remove_base_path(local_dir, base_path);
          add_info_to_cmd_options(local_dir, cmd, help);
      }
  } while ((entry = readdir(dir)) && (entry != NULL));
  closedir(dir);
}

/*
 * Function        : vtysh_config_context_timezone_callback
 * Resposibility   : Handler to provide timezone with show running-config
 */
vtysh_ret_val
vtysh_config_context_timezone_callback(void *p_private)
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

/* Initialize ops-sysd cli nodd.
 */
void
cli_pre_init(void)
{
   system_ovsdb_init();
   system_install_timezone_set_command();
}

/* Initialize ops-sysd cli element.
 */
void
cli_post_init(void)
{
    int retval;

    install_element (ENABLE_NODE, &cli_platform_show_system_cmd);
    install_element (VIEW_NODE, &cli_platform_show_system_cmd);
    install_element (VIEW_NODE, &vtysh_show_system_clock_cmd);
    install_element (ENABLE_NODE, &vtysh_show_system_clock_cmd);
    install_element (VIEW_NODE, &vtysh_show_date_cmd);
    install_element (ENABLE_NODE, &vtysh_show_date_cmd);
    install_element (VIEW_NODE, &vtysh_show_system_timezone_cmd);
    install_element (ENABLE_NODE, &vtysh_show_system_timezone_cmd);

    /* Installing running config sub-context with global config context */
    retval = install_show_run_config_subcontext(e_vtysh_config_context,
                                     e_vtysh_config_context_global,
                                     &vtysh_config_context_system_callback,
                                     NULL, NULL);
    if(e_vtysh_ok != retval)
    {
        vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                           "config context unable to add system timezone callback");
        assert(0);
    }

}
