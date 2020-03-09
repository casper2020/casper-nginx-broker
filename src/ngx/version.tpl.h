/**
 * @file version.h
 *
 * Copyright (c) 2017-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-nginx-broker.
 *
 * casper-nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-nginx-broker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-nginx-broker. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_NGX_VERSION_H_
#define NRS_NGX_VERSION_H_

#include "core/nginx.h"

#ifndef NGX_ABBR
#define NGX_ABBR "nb"
#endif

#ifndef NGX_NAME
#define NGX_NAME "nginx-broker"
#endif

#ifndef NGX_VERSION
#define NGX_VERSION "x.x.x"
#endif

#ifndef NGX_INFO
#define NGX_INFO NGX_NAME " v" NGX_VERSION "/" NGINX_VERSION
#endif

#endif // NRS_NGX_VERSION_H_
