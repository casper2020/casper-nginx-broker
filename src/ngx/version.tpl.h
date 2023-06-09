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
#define NGX_NAME "n.n.n"
#endif

#ifndef NGX_VERSION
#define NGX_VERSION "x.x.x"
#endif

#ifndef NGX_SHARED_DIR
#define NGX_SHARED_DIR "s.s.s"
#endif

#ifndef NGX_ETC_DIR
#define NGX_ETC_DIR "e.t.c"
#endif

#ifndef NGX_REL_DATE
#define NGX_REL_DATE "r.r.d"
#endif

#ifndef NGX_REL_BRANCH
#define NGX_REL_BRANCH "r.r.b"
#endif

#ifndef NGX_REL_HASH
#define NGX_REL_HASH "r.r.h"
#endif

#ifndef NGX_REL_TARGET
#define NGX_REL_TARGET "r.r.t"
#endif

#ifndef NGX_INFO
#define NGX_INFO NGX_NAME " v" NGX_VERSION "/" NGINX_VERSION
#endif

#ifndef NGX_VARIANT
#define NGX_VARIANT v.v.v
#endif

#ifndef NGX_BANNER
  #if 0 == NGX_VARIANT
    #define NGX_BANNER \
      " _   _    ____   ___   _   _  __  __" \
      "\n| \\ | |  / ___| |_ _| | \\ | | \\ \\/ /" \
      "\n|  \\| | | |  _   | |  |  \\| |  \\  /" \
      "\n| |\\  | | |_| |  | |  | |\\  |  /  \\" \
      "\n|_| \\_|  \\____| |___| |_| \\_| /_/\\_\\"
  #elif 1 == NGX_VARIANT
    #define NGX_BANNER \
      " _   _    ____   ___   _   _  __  __    ____    ____     ___    _  __  _____   ____  " \
      "\n| \\ | |  / ___| |_ _| | \\ | | \\ \\/ /   | __ )  |  _ \\   / _ \\  | |/ / | ____| |  _ \\ " \
      "\n|  \\| | | |  _   | |  |  \\| |  \\  /    |  _ \\  | |_) | | | | | | ' /  |  _|   | |_) |" \
      "\n| |\\  | | |_| |  | |  | |\\  |  /  \\    | |_) | |  _ <  | |_| | | . \\  | |___  |  _ < " \
      "\n|_| \\_|  \\____| |___| |_| \\_| /_/\\_\\   |____/  |_| \\_\\  \\___/  |_|\\_\\ |_____| |_| \\_\\"
  #elif 2 == NGX_VARIANT
    #define NGX_BANNER \
      "\n _   _    ____   ___   _   _  __  __    _____   ____  " \
      "\n| \\ | |  / ___| |_ _| | \\ | | \\ \\/ /   |  ___| / ___| " \
      "\n|  \\| | | |  _   | |  |  \\| |  \\  /    | |_    \\___ \\ " \
      "\n| |\\  | | |_| |  | |  | |\\  |  /  \\    |  _|    ___) |" \
      "\n|_| \\_|  \\____| |___| |_| \\_| /_/\\_\\   |_|     |____/ "
  #elif 3 == NGX_VARIANT
    #define NGX_BANNER \
      " _   _    ____   ___   _   _  __  __     ____   ____    ____  " \
      "\n| \\ | |  / ___| |_ _| | \\ | | \\ \\/ /    / ___| |  _ \\  | __ ) " \
      "\n|  \\| | | |  _   | |  |  \\| |  \\  /    | |     | | | | |  _ \\ " \
      "\n| |\\  | | |_| |  | |  | |\\  |  /  \\    | |___  | |_| | | |_) |" \
      "\n|_| \\_|  \\____| |___| |_| \\_| /_/\\_\\    \\____| |____/  |____/ "
  #elif 4 == NGX_VARIANT
    #define NGX_BANNER \
      "_   _  ____ ___ _   ___  __     _   _ ____  __  __" \
      "\n| \\ | |/ ___|_ _| \\ | \\ \\/ /    | | | / ___||  \\/  |" \
      "\n|  \\| | |  _ | ||  \\| |\\  /_____| |_| \\___ \\| |\\/| |" \
      "\n| |\\  | |_| || || |\\  |/  \\_____|  _  |___) | |  | |" \
      "\n|_| \\_|\\____|___|_| \\_/_/\\_\\    |_| |_|____/|_|  |_|"
  #else
    #define NGX_BANNER ""
  #endif
#endif

#endif // NRS_NGX_VERSION_H_
