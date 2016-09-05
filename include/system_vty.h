/* System CLI commands.
 *
 * Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * File: system_vty.h
 *
 * Purpose: To add system CLI configuration and display commands.
 */

#ifndef _SYSTEM_VTY_H
#define _SYSTEM_VTY_H

#ifndef SYS_STR
#define SYS_STR	         "System information\n"
#endif
#define CLOCK_STR  "Shows system clock information\n"
#define DATE_STR  "Shows system date information\n"
#define TIMEZONE_STR "Timezone information\n"
#define TIMEZONE_SET_STR "Sets Timezone configuration\n"
#define TIMEZONE_PLACEHOLDER_STR "Timezone Info\n"
#define DEFAULT_TIMEZONE "utc"
#define MAX_TIMEZONES          900
#define MAX_TIMEZONE_NAME_SIZE 100
#define MAX_TIMEZONE_CMD_SIZE  (MAX_TIMEZONES*MAX_TIMEZONE_NAME_SIZE)
#define MAX_TIMEZONE_HELP_SIZE 100
#define MAX_TIMEZONES_HELP_SIZE  (MAX_TIMEZONES*MAX_TIMEZONE_HELP_SIZE)
typedef enum
{
        CLI_FAN,
        CLI_PSU,
        CLI_LED,
        CLI_TEMP
}cli_subsystem;

int cli_system_get_all();

void cli_pre_init(void);
void cli_post_init(void);

static int zone_count = 0;
static char base_path[100] = "/usr/share/zoneinfo/posix/";
char list_of_zones[MAX_TIMEZONES][MAX_TIMEZONE_NAME_SIZE];
char list_of_zones_caps[MAX_TIMEZONES][MAX_TIMEZONE_NAME_SIZE];
void find_posix_timezones_and_add_to_list(const char *name, int level, char *cmd, char *help);

#endif //_SYSTEM_VTY_H
