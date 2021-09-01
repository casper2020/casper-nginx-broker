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

THIS_DIR := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ifeq (casper-nginx-broker, $(shell basename $(THIS_DIR)))
  PACKAGER_DIR := $(abspath $(THIS_DIR)/../casper-packager)
else
  PACKAGER_DIR := $(abspath $(THIS_DIR)/..)
endif

include $(PACKAGER_DIR)/common/c++/settings.mk

REL_VARIANT          ?= 1
REL_NAME             ?= nginx-broker
REL_DATE             := $(shell date -u)
REL_HASH             := $(shell git rev-parse HEAD)
REL_BRANCH           := $(shell git rev-parse --abbrev-ref HEAD)

PROJECT_SRC_DIR      := $(ROOT_DIR)/casper-nginx-broker
EXECUTABLE_NAME      := 
EXECUTABLE_MAIN_SRC  :=
LIBRARY_TYPE         := static
LIBRARY_NAME         := libbroker.a
VERSION              := $(shell cat $(PACKAGER_DIR)/$(REL_NAME)/version)
CHILD_CWD            := $(THIS_DIR)
CHILD_MAKEFILE       := $(firstword $(MAKEFILE_LIST))

# special case: this lib pulls all dependencies ( acts as a central point of configuration )
PROCESS_DEPENDENCIES := true

############################                                                                                                                                                                                                                  
# BROKER VARIABLES                                                                                                                                                                                                                              
############################    

include $(PROJECT_SRC_DIR)/common.mk

############################
# COMMON REQUIRED VARIABLES
############################

CC_SRC := \
  $(BROKER_CC_SRC)

BISON_SRC := \
  $(BROKER_YY_SRC)

RAGEL_SRC := \
  $(BROKER_RL_SRC)

INCLUDE_DIRS := \
  $(BROKER_INCLUDE_DIRS)

OBJECTS := \
  $(BISON_SRC:.yy=.o) \
  $(RAGEL_SRC:.rl=.o) \
  $(CC_SRC:.cc=.o)

include $(PACKAGER_DIR)/common/c++/common.mk

CASPER_NGINX_BROKER_DEPENDENCIES := \
  casper-connectors-icu-dep-on casper-osal-icu-dep-on \
  openssl-dep-on \
  cppcodec-dep-on \
  zlib-dep-on libbcrypt-dep-on beanstalk-client-dep-on postgresql-dep-on curl-dep-on jsoncpp-dep-on cppcodec-dep-on hiredis-dep-on \
  libevent2-dep-on

.SECONDEXPANSION: update-objects
update-objects: update-sources
	@$(eval CC_SRC:=$(BROKER_CC_SRC))
	@$(eval BISON_SRC:=$(BROKER_YY_SRC))
	@$(eval RAGEL_SRC:=$(BROKER_RL_SRC))
	@$(eval OBJECTS:=$(BISON_SRC:.yy=.o) $(RAGEL_SRC:.rl=.o) $(CC_SRC:.cc=.o))

.SECONDEXPANSION: set-dependencies
set-dependencies: $(CASPER_NGINX_BROKER_DEPENDENCIES) update-objects

# version
version:
	@echo " $(LOG_COMPILING_PREFIX) - patching $(PROJECT_SRC_DIR)/src/ngx/version.h"
	@cp -f $(PROJECT_SRC_DIR)/src/ngx/version.tpl.h $(PROJECT_SRC_DIR)/src/ngx/version.h
	@sed -i.bak s#"x.x.x"#$(VERSION)#g $(PROJECT_SRC_DIR)/src/ngx/version.h
	@sed -i.bak s#"n.n.n"#$(REL_NAME)#g $(PROJECT_SRC_DIR)/src/ngx/version.h
	@sed -i.bak s#"r.r.d"#"$(REL_DATE)"#g $(PROJECT_SRC_DIR)/src/ngx/version.h
	@sed -i.bak s#"r.r.b"#"$(REL_BRANCH)"#g $(PROJECT_SRC_DIR)/src/ngx/version.h
	@sed -i.bak s#"r.r.h"#"$(REL_HASH)"#g $(PROJECT_SRC_DIR)/src/ngx/version.h
	@sed -i.bak s#"v.v.v"#"$(REL_VARIANT)"#g $(PROJECT_SRC_DIR)/src/ngx/version.h
	@rm -f $(PROJECT_SRC_DIR)/src/ngx/version.h.bak

all: lib

.SECONDARY:
