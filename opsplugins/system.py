#!/usr/bin/env python
# Copyright (C) 2016 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

from opsvalidator.base import BaseValidator
from opsvalidator import error
from opsvalidator.error import ValidationError
from opsrest.utils.utils import get_column_data_from_row

import os
from copy import copy
global list_of_timezones
list_of_timezones = None


def build_timezone_db():
    global list_of_timezones
    path = "/usr/share/zoneinfo/posix/"
    for root, directories, filenames in os.walk(path):
        for filename in filenames:
            full_path = os.path.join(root, filename)
            timezone = copy(full_path)
            timezone = timezone.replace(path, "")
            list_of_timezones[timezone.lower()] = full_path


def check_valid_timezone(timezone_user_input):
    global list_of_timezones
    if list_of_timezones is None:
        list_of_timezones = {}
        build_timezone_db()
    if timezone_user_input in list_of_timezones.keys():
        return True
    else:
        return False


class SystemValidator(BaseValidator):
    resource = "system"

    def validate_modification(self, validation_args):
        system_row = validation_args.resource_row
        if hasattr(system_row, "timezone"):
            timezone = get_column_data_from_row(system_row, "timezone")
            if (check_valid_timezone(timezone) is False):
                details = "Invalid timezone %s." % (timezone)
                raise ValidationError(error.VERIFICATION_FAILED, details)
