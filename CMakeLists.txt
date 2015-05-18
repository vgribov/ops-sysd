# Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
# sysd/CMakeLists.txt

cmake_minimum_required (VERSION 2.8)

set (SYSD sysd)
project (${SYSD})

set (SYSCONFDIR "/etc" CACHE STRING "Location of system configuration files")
set (HWDESC_FILE_LINK_PATH ${SYSCONFDIR}/halon)
set (HWDESC_FILE_LINK ${SYSCONFDIR}/halon/hwdesc)
set (HWDESC_FILES_PATH ${SYSCONFDIR}/halon/platform)

# Update the platform YAML config file location in sysd_cfgyaml
configure_file (${PROJECT_SOURCE_DIR}/sysd_cfg_yaml.h.in
                ${PROJECT_BINARY_DIR}/sysd_cfg_yaml.h)

set (MANIFEST_FILE_PATH /etc/halon/image.manifest)

# Update the image.manifest file location in sysd_utils
configure_file (${PROJECT_SOURCE_DIR}/sysd_util.h.in
                ${PROJECT_BINARY_DIR}/sysd_util.h)

# Rules to locate needed libraries
include(FindPkgConfig)
pkg_check_modules(OPENSSL openssl)
pkg_check_modules(SYSTEMD systemd)
pkg_check_modules(ZLIB zlib)

pkg_check_modules(CONFIG_YAML config-yaml)
pkg_check_modules(HALONUTILS halonutils)
pkg_check_modules(OVSCOMMON libovscommon)
pkg_check_modules(OVSDB libovsdb)

include_directories (${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}
                     ${OVSCOMMON_INCLUDE_DIRS})

# Source files to build sysd
set (SOURCES sysd.c sysd_cfg_yaml.c sysd_fru.c sysd_ovsdb_if.c sysd_util.c)

# Rules to build sysd
add_executable (${SYSD} ${SOURCES})

target_link_libraries (${SYSD} ${HALONUTILS_LIBRARIES} ${CONFIG_YAML_LIBRARIES}
                       ${OVSCOMMON_LIBRARIES} ${OVSDB_LIBRARIES}
                       ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES}
                       ${SYSTEMD_LIBRARIES} -lpthread -lrt)

# The default install prefix is /usr. We want to install manifest file at
# '/etc/halon'. So change the install prefix to '/', and use relative
# directories in the install command.
SET(CMAKE_INSTALL_PREFIX "/" CACHE STRING "Prefix prepended to install directories" FORCE)

# Rules to install sysd binary in rootfs
install(TARGETS ${SYSD}
        RUNTIME DESTINATION usr/bin)

# HALON_TODO: The image.manifest file should not be located in sysd.
# This is just temporary parking space until we find it better home.
install(FILES image.manifest
        DESTINATION etc/halon)
