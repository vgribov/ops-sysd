/************************************************************************//**
 * (c) Copyright 2015 Hewlett Packard Enterprise Development LP
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
/* @ingroup ops-sysd
 *
 * @file
 * Header for Stub X86-64 FRU EEPROM info.
 */

#ifndef __SYSD_STUB_X86_FRU_H__
#define __SYSD_STUB_X86_FRU_H__

/** @ingroup ops-sysd
 * @{ */

/* Generic-x86-64 stub eeprom info */
#define STUB_X86_64_COUNTRY_CODE        "US"
#define STUB_X86_64_DEVICE_VERSION      0
#define STUB_X86_64_DIAG_VERSION        "1.0.0.0"
#define STUB_X86_64_LABEL_VERSION       "L01"
#define STUB_X86_64_MANUFACTURE_DATE    "09/01/2015 00:00:01"
#define STUB_X86_64_MANUFACTURER        "OpenSwitch"
#define STUB_X86_64_NUM_MAC             74
#define STUB_X86_64_ONIE_VERSION        "2014.08.00.05"
#define STUB_X86_64_PART_NUMBER         "OPSX8664"
#define STUB_X86_64_PLATFORM_NAME       "Generic-x86-64"
#define STUB_X86_64_PRODUCT_NAME        "OpenSwitch"
#define STUB_X86_64_SERIAL_NUMBER       "X8664001"
#define STUB_X86_64_SERVICE_TAG         0
#define STUB_X86_64_VENDOR              "OpenSwitch"

extern bool sysd_stub_x86_64_eeprom_info(fru_eeprom_t *fru_eeprom);

/** @} end of group ops-sysd */
#endif /* __SYSD_STUB_X86_FRU_H__ */
