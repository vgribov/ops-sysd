/* vim: ai:ts=4:sw=4
 */
/************************************************************************//**
 * @ingroup sysd
 *
 * @file
 * Header for sysd OVSDB access functions.
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

#ifndef __SYSD_OVSDB_IF_H__
#define __SYSD_OVSDB_IF_H__

/** @ingroup sysd
 * @{ */

#define SYSD_MAC_FORMAT(a)	a[0], a[1], a[2], a[3], a[4], a[5]

#define SYSD_OVS_PTR_CALLOC(OVS_STR, count)		\
			(struct  OVS_STR *) calloc(sizeof(struct OVS_STR), count)

void sysd_run(void);
void sysd_wait(void);

/** @} end of group sysd */
#endif /* __SYSD_OVSDB_IF_H__ */

