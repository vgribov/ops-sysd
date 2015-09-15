/* vim: ai:ts=4:sw=4:expandtab
 */
/************************************************************************//**
 * @ingroup ops-sysd
 *
 * @file
 * Header for ops-sysd FRU EEPROM access functions.
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

#ifndef __SYSD_FRU_H__
#define __SYSD_FRU_H__

/** @ingroup ops-sysd
 * @{ */

#include <stdint.h>

#define SUPPORTED_OCP_FRU_EEPROM_VERSION    0x01

#define FRU_COUNTRY_CODE_LEN            2
#define FRU_COUNTRY_CODE_TYPE           0x2c
#define FRU_DEVICE_VERSION_LEN          1
#define FRU_DEVICE_VERSION_TYPE         0x26
#define FRU_DIAG_VERSION_TYPE           0x2e
#define FRU_LABEL_REVISION_TYPE         0x27
#define FRU_BASE_MAC_ADDRESS_LEN        6
#define FRU_BASE_MAC_ADDRESS_TYPE       0x24
#define FRU_MANUFACTURE_DATE_LEN        19
#define FRU_MANUFACTURE_DATE_TYPE       0x25
#define FRU_MANUFACTURER_TYPE           0x2b
#define FRU_NUM_MACS_LEN                2
#define FRU_NUM_MAC_TYPE                0x2a
#define FRU_ONIE_VERSION_TYPE           0x29
#define FRU_PART_NUMBER_TYPE            0x22
#define FRU_PLATFORM_NAME_TYPE          0x28
#define FRU_PRODUCT_NAME_TYPE           0x21
#define FRU_SERIAL_NUMBER_TYPE          0x23
#define FRU_SERVICE_TAG_TYPE            0x2f
#define FRU_VENDOR_TYPE                 0x2d
#define FRU_VENDOR_EXTENSION_TYPE       0xfd
#define FRU_CRC_TYPE                    0xfe

#define FRU_CRC_LEN                     6

typedef struct fru_eeprom {
    char        country_code[FRU_COUNTRY_CODE_LEN+1];
    char        device_version;
    char        *diag_version;
    char        *label_revision;
    uint8_t     base_mac_address[FRU_BASE_MAC_ADDRESS_LEN];
    char        manufacture_date[FRU_MANUFACTURE_DATE_LEN+1];
    char        *manufacturer;
    uint16_t    num_macs;
    char        *onie_version;
    char        *part_number;
    char        *platform_name;
    char        *product_name;
    char        *serial_number;
    char        *service_tag;
    char        *vendor;
} fru_eeprom_t;

/* local structs */
typedef struct fru_header {
    char            id[8];
    uint8_t         header_version;
    unsigned char   total_length[2];
} fru_header_t;

typedef struct fru_tlv {
    uint8_t         code;
    uint8_t         length;
    char            value[255];
} fru_tlv_t;

int sysd_read_fru_eeprom(fru_eeprom_t *fru_eeprom);

/** @} end of group ops-sysd */
#endif /* __SYSD_FRU_H__ */
