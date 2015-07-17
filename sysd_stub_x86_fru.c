/* vim: ai:ts=4:sw=4:expandtab
 */
/************************************************************************//**
 * @ingroup sysd
 *
 * @file
 * Source for sysd stub generic-x86-64 FRU EEPROM interface.
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

#include <string.h>

#include <openvswitch/vlog.h>

#include "sysd_fru.h"
#include "sysd_stub_x86_fru.h"

VLOG_DEFINE_THIS_MODULE(x86_fru);

/** @ingroup sysd
 * @{ */

uint8_t stub_x86_64_base_mac_address[6] =
                          { 0x70, 0x72, 0xcf, 0xf5, 0xfa, 0xd6 };

bool
sysd_stub_x86_64_eeprom_info(fru_eeprom_t *fru_eeprom)
{

    strncpy(fru_eeprom->country_code, STUB_X86_64_COUNTRY_CODE,
                                       FRU_COUNTRY_CODE_LEN);
    fru_eeprom->country_code[FRU_COUNTRY_CODE_LEN] = '\0';

    fru_eeprom->device_version = STUB_X86_64_DEVICE_VERSION;

    fru_eeprom->diag_version = strdup(STUB_X86_64_DIAG_VERSION);

    fru_eeprom->label_revision = strdup(STUB_X86_64_LABEL_VERSION);

    memcpy(fru_eeprom->base_mac_address, stub_x86_64_base_mac_address,
           FRU_BASE_MAC_ADDRESS_LEN);

    strncpy(fru_eeprom->manufacture_date, STUB_X86_64_MANUFACTURE_DATE,
            FRU_MANUFACTURE_DATE_LEN);
    fru_eeprom->manufacture_date[FRU_MANUFACTURE_DATE_LEN] = '\0';

    fru_eeprom->manufacturer = strdup(STUB_X86_64_MANUFACTURER);

    fru_eeprom->num_macs = STUB_X86_64_NUM_MAC;

    fru_eeprom->onie_version = strdup(STUB_X86_64_ONIE_VERSION);

    fru_eeprom->part_number = strdup(STUB_X86_64_PART_NUMBER);

    fru_eeprom->platform_name = strdup(STUB_X86_64_PLATFORM_NAME);

    fru_eeprom->product_name = strdup(STUB_X86_64_PRODUCT_NAME);

    fru_eeprom->serial_number = strdup(STUB_X86_64_SERIAL_NUMBER);

    fru_eeprom->service_tag = STUB_X86_64_SERVICE_TAG;

    fru_eeprom->vendor = strdup(STUB_X86_64_VENDOR);

    return (true);
} /* sysd_stub_x86_eeprom_info() */
/** @} end of group sysd */
