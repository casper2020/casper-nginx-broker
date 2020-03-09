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
# casper-nginx-broker  is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with casper-nginx-broker. If not, see <http://www.gnu.org/licenses/>.
#

THIS_DIR := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ifeq (casper-nginx-broker, $(shell basename $(THIS_DIR)))
  PACKAGER_DIR := $(abspath $(THIS_DIR)/../casper-packager)
else
  PACKAGER_DIR := $(abspath $(THIS_DIR)/..)
endif

include $(PACKAGER_DIR)/common/c++/settings.mk

PROJECT_SRC_DIR     := $(ROOT_DIR)/casper-nginx-broker
EXECUTABLE_NAME     := nb-xattr
EXECUTABLE_MAIN_SRC := $(PROJECT_SRC_DIR)/src/xattr/nb-xattr.cc
LIBRARY_NAME        :=
VERSION             ?= $(shell cat $(PACKAGER_DIR)/nb-xattr-2/version)
CHILD_CWD           := $(THIS_DIR)
CHILD_MAKEFILE      := $(MAKEFILE_LIST)

include common.mk

NB_XATTR_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/xattr/archive.cc                         \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/act.cc      \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/ast/tree.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/archive.cc

NB_XATTR_YY_SRC := \
   $(BROKER_CDN_COMMON_MODULE_YY_SRC)

NB_XATTR_RL_SRC := \
   $(BROKER_CDN_COMMON_MODULE_RL_SRC)

CC_SRC := \
  $(NB_XATTR_CC_SRC)

BISON_SRC := \
  $(NB_XATTR_YY_SRC)

RAGEL_SRC := \
  $(NB_XATTR_RL_SRC)

INCLUDE_DIRS := \
  -I $(PROJECT_SRC_DIR)/src \
  $(BROKER_NGX_INCLUDE_DIRS)

OBJECTS := \
  $(BISON_SRC:.yy=.o) \
  $(RAGEL_SRC:.rl=.o) \
  $(CC_SRC:.cc=.o)    \
  $(CPP_SRC:.cpp=.o)

###################
# OTHER VARIABLES
###################

include $(PACKAGER_DIR)/common/c++/common.mk

set-dependencies: jsoncpp-dep-on casper-osal-dep-on casper-connectors-dep-on cppcodec-dep-on

all: exec

# version
version:
	@echo " $(LOG_COMPILING_PREFIX) - patching $(PROJECT_SRC_DIR)/src/xattr/version.h"
	@cp -f $(PROJECT_SRC_DIR)/src/xattr/version.tpl.h $(PROJECT_SRC_DIR)/src/xattr/version.h
	@sed -i.bak s#"x.x.xx"#$(VERSION)#g $(PROJECT_SRC_DIR)/src/xattr/version.h
	@rm -f $(PROJECT_SRC_DIR)/src/xattr/version.h.bak

.SECONDARY:
