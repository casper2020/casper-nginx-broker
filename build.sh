#!/bin/bash
#
# Copyright (c) 2017-2020 Cloudware S.A. All rights reserved.
#
# This file is part of casper-nginx-broker.
#
# casper-nginx-broker is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# casper-nginx-broker is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with casper-nginx-broker. If not, see <http://www.gnu.org/licenses/>.
#

TMP_PWD=`pwd`
TMP_THIS_SCRIPT_FILE_NAME=${PWD##*/}/$(basename $0)
THIS_SCRIPT_FILE_NAME=${TMP_THIS_SCRIPT_FILE_NAME%.*}

#############################################
# Include other scripts.
#############################################

source "../casper-packager/common/common.inc.sh"
if [ $? -ne 0 ]; then
    exit -1
fi

source "../casper-packager/common/repos.inc.sh"
if [ $? -ne 0 ]; then
    exit -1
fi

#############################################
# Set this script global variables.
#############################################

CURRENT_RELATIVE_PWD=$(dirname "$BASH_SOURCE")/

#############################################
# MAIN
#############################################

# Parse arguments
parse_arguments "$@"

# collect info
collect_platform_info

# Process request
if [ "build" == "${ACTION}" ] || [ "rebuild" == "${ACTION}" ] || [ "configure" == "${ACTION}" ] ; then
  : # pass
else
    echo "Invalid params:"
    print_usage_and_die $0
fi

TMP_VERBOSE=
if [ -z "${SILENT}" ] || [ " " == "${SILENT}" ] ; then
    TMP_VERBOSE='-v'
else
    TMP_VERBOSE=''
fi

if [ "Darwin" == "$PLATFORM" ] ; then
    MAKE_PACKAGE_ARG="-p"
    DEVELOPMENT_ARG="-d"
else
    MAKE_PACKAGE_ARG=
    DEVELOPMENT_ARG=
fi

# dec
if [ "build" == "${ACTION}" ] || [ "rebuild" == "${ACTION}" ] ; then
    exec_or_die "( cd ../casper-packager && bash nginx-broker/build.sh -t ${TARGET} -a ${ACTION} ${TMP_VERBOSE} ${DEVELOPMENT_ARG} ${MAKE_PACKAGE_ARG})"
    # clean
    exec_or_die "xcodebuild -project nginx-broker.xcodeproj -configuration ${TARGET_CC} clean -scheme nginx-broker > /dev/null 2>&1"
    exit 0
fi

# how to purge
# @ casper-nginx-broker dir:
# rm -rf ./out
# rm -rf ../casper-packager/nginx-broker/darwin/
# rm -rf /usr/local/bin/nginx-broker
# rm -rf /usr/local/etc/nginx-broker
# rm -rf /usr/local/share/nginx-broker
# rm -rf /usr/local/var/log/nginx-broker
# rm -rf /usr/local/var/nginx-broker
# rm -rf /usr/local/var/tmp/nginx-broker
