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

############################
# BROKER VARIABLES
############################

BROKER_CDN_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn/module.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn/errors.cc

BROKER_CDN_COMMON_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/errors.cc     \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/act.cc        \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/archive.cc    \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/ast/tree.cc   \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/db/billing.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/db/object.cc  \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/db/sideline.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/module.cc

BROKER_CDN_COMMON_MODULE_YY_SRC := \
	$(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/ast/parser.yy

BROKER_CDN_COMMON_MODULE_RL_SRC := \
	$(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/ast/evaluator.rl \
	$(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-common/ast/scanner.rl

BROKER_CDN_ARCHIVE_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-archive/db/synchronization.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-archive/module.cc

BROKER_CDN_API_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-api/route.cc    \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-api/billing.cc  \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-api/sideline.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-api/module.cc

BROKER_CDN_API_MODULE_RL_SRC := \
	$(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-api/router.rl

BROKER_CDN_REPLICATOR_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-replicator/module.cc

BROKER_CDN_DOWNLOAD_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/cdn-download/module.cc

BROKER_OAUTH_SERVER_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/abstract_token.cc     \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/access_token.cc       \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/authorization_code.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/scope.cc              \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/errors.cc             \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/module.cc             \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/object.cc             \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/oauth/server/refresh_token.cc

BROKER_API_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/api/errors.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/api/module.cc

BROKER_JWT_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/jwt/encoder/errors.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/jwt/encoder/module.cc

BROKER_JOBIFY_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/jobify/errors.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/jobify/module.cc

BROKER_UL_MODULE_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/ul/errors.cc

BROKER_NGX_INCLUDE_DIRS := \
  -I $(PROJECT_SRC_DIR)/src                         \
  -I $(PROJECT_SRC_DIR)/src/nginx/src               \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/event         \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/core          \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/http          \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/http/v2       \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/http/modules  \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/event/modules \
  -I $(PROJECT_SRC_DIR)/src/nginx/src/os/unix       \
  -I $(PROJECT_SRC_DIR)/src/nginx/objs

BROKER_INCLUDE_DIRS := \
  $(BROKER_NGX_INCLUDE_DIRS)

BROKER_CC_SRC := \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/executor.cc    \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/initializer.cc \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/module.cc      \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/ext/base.cc    \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/ext/job.cc     \
  $(PROJECT_SRC_DIR)/src/ngx/casper/broker/ext/session.cc \
  $(PROJECT_SRC_DIR)/src/ngx/ngx_utils.cc                 \
  $(PROJECT_SRC_DIR)/src/ngx/casper/ev/glue.cc            \

BROKER_YY_SRC :=
BROKER_RL_SRC :=

#### #### #### #### ####
### OPTIONAL MODULES 
#### #### #### #### ####

CASPER_NGINX_BROKER_API_MODULE_DEP_ON?=false
CASPER_NGINX_BROKER_OAUTH_SERVER_MODULE_DEP_ON?=false
CASPER_NGINX_BROKER_UL_MODULE_DEP_ON?=false
CASPER_NGINX_BROKER_JWT_ENCODER_MODULE_DEP_ON?=false
CASPER_NGINX_BROKER_JOBIFY_MODULE_DEP_ON?=false
CASPER_NGINX_BROKER_CDN_MODULE_DEP_ON?=false

NGX_CDB_MODULES_DEP_ON?=false
NAMED_IMPORTS_NGINX_MODULE_DEP_ON?=false

.SECONDEXPANSION: update-sources
update-sources:
	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_API_MODULE_DEP_ON) ] ; then echo "$(BROKER_API_MODULE_CC_SRC)"; fi))
	@$(eval BROKER_RL_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_API_MODULE_DEP_ON) ] ; then echo "$(BROKER_CDN_API_MODULE_RL_SRC)"; fi))

	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_OAUTH_SERVER_MODULE_DEP_ON) ] ; then echo "$(BROKER_OAUTH_SERVER_MODULE_CC_SRC)"; fi))

	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_UL_MODULE_DEP_ON) ] ; then echo "$(BROKER_UL_MODULE_CC_SRC)"; fi))

	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_JWT_ENCODER_MODULE_DEP_ON) ] ; then echo "$(BROKER_JWT_MODULE_CC_SRC)"; fi))

	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_JOBIFY_MODULE_DEP_ON) ] ; then echo "$(BROKER_JOBIFY_MODULE_CC_SRC)"; fi))

	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_CDN_MODULE_DEP_ON) ] ; then \
		echo "$(BROKER_CDN_COMMON_MODULE_CC_SRC) $(BROKER_CDN_MODULE_CC_SRC) $(BROKER_CDN_ARCHIVE_MODULE_CC_SRC) $(BROKER_CDN_REPLICATOR_MODULE_CC_SRC) $(BROKER_CDN_DOWNLOAD_MODULE_CC_SRC) $(BROKER_CDN_API_MODULE_CC_SRC)" ; \
		fi \
	))
	@$(eval BROKER_YY_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_CDN_MODULE_DEP_ON) ] ; then echo "$(BROKER_CDN_COMMON_MODULE_YY_SRC)" ; fi))
	@$(eval BROKER_RL_SRC+=$(shell if [ true = $(CASPER_NGINX_BROKER_CDN_MODULE_DEP_ON) ] ; then echo "$(BROKER_CDN_COMMON_MODULE_RL_SRC)" ; fi))

	@$(eval BROKER_CC_SRC+=$(shell if [ true = $(NAMED_IMPORTS_NGINX_MODULE_DEP_ON) ] ; then echo "$(NAMED_IMPORTS_MODULE_CC_SRC)"; fi))
	@$(eval BROKER_RL_SRC+=$(shell if [ true = $(NAMED_IMPORTS_NGINX_MODULE_DEP_ON) ] ; then echo "$(NAMED_IMPORTS_MODULE_RL_SRC)"; fi))
	@$(eval BROKER_INCLUDE_DIRS+=$(shell if [ true = $(NAMED_IMPORTS_NGINX_MODULE_DEP_ON) ] ; then echo "$(NAMED_IMPORTS_MODULE_INCLUDE_DIRS)"; fi))

	@$(eval BROKER_INCLUDE_DIRS+=$(shell if [ true = $(NGX_CDB_MODULES_DEP_ON) ] ; then echo "$(NGX_CDB_MODULES_INCLUDE_DIRS)"; fi))

### named-imports-nginx-module
ifeq (true, $(NAMED_IMPORTS_NGINX_MODULE_DEP_ON))
  include ../named-imports-nginx-module/common.mk
  # BROKER_CC_SRC += $(NAMED_IMPORTS_MODULE_CC_SRC)
  # BROKER_RL_SRC += $(NAMED_IMPORTS_MODULE_RL_SRC)
  # BROKER_INCLUDE_DIRS += $(NAMED_IMPORTS_MODULE_INCLUDE_DIRS)
endif

### cdb
ifeq (true, $(NGX_CDB_MODULES_DEP_ON))
  include $(NGX_CDB_MODULES_SRC_DIR)/common.mk
#  BROKER_INCLUDE_DIRS += $(NGX_CDB_MODULES_INCLUDE_DIRS)
endif
