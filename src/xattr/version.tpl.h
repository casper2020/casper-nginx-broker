/**
 * @file version.h
 *
 * Copyright (c) 2017-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_XATTR_VERSION_H_
#define NRS_XATTR_VERSION_H_

#ifndef NRS_CASPER_NGINX_BROKER_XATTR_NAME
#define NRS_CASPER_NGINX_BROKER_XATTR_NAME "nb-xattr@b.n.s@"
#endif

#ifndef NRS_CASPER_NGINX_BROKER_XATTR_VERSION
#define NRS_CASPER_NGINX_BROKER_XATTR_VERSION "x.x.x"
#endif

#ifndef NRS_CASPER_NGINX_BROKER_XATTR_REL_DATE
#define NRS_CASPER_NGINX_BROKER_XATTR_REL_DATE "r.r.d"
#endif

#ifndef NRS_CASPER_NGINX_BROKER_XATTR_REL_BRANCH
#define NRS_CASPER_NGINX_BROKER_XATTR_REL_BRANCH "r.r.b"
#endif

#ifndef NRS_CASPER_NGINX_BROKER_XATTR_REL_VERSION
#define NRS_CASPER_NGINX_BROKER_XATTR_REL_VERSION "r.r.h"
#endif

#ifndef NRS_CASPER_NGINX_BROKER_XATTR_INFO
#define NRS_CASPER_NGINX_BROKER_XATTR_INFO NRS_CASPER_NGINX_BROKER_XATTR_NAME " v" NRS_CASPER_NGINX_BROKER_XATTR_VERSION
#endif

#ifndef NGX_BANNER
  #define NGX_BANNER \
    " _   _   ____           __  __     _      _____   _____   ____  " \
    "\n| \\ | | | __ )          \\ \\/ /    / \\    |_   _| |_   _| |  _ \\ " \
    "\n|  \\| | |  _ \\   _____   \\  /    / _ \\     | |     | |   | |_) |" \
    "\n| |\\  | | |_) | |_____|  /  \\   / ___ \\    | |     | |   |  _ < " \
    "\n|_| \\_| |____/          /_/\\_\\ /_/   \\_\\   |_|     |_|   |_| \\_\\"
#endif

#endif // NRS_XATTR_VERSION_H_
