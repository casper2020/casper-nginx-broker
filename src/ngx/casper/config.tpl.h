/**
 * @file config.h
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
#ifndef NRS_NGX_CASPER_CONFIG_H_
#define NRS_NGX_CASPER_CONFIG_H_

#define NGX_IS_CASPER_NGINX_HSM_ENABLED 1
@MODULES@
@THIRD_PARTY_MODULES@
#endif // NRS_NGX_CASPER_CONFIG_H_
