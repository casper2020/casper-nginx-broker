ngx_addon_name=ngx_http_casper_broker_cdn_public_module

HTTP_MODULES="$HTTP_MODULES ngx_http_casper_broker_cdn_public_module"

NGX_ADDON_SRCS="$NGX_ADDON_SRCS \
$ngx_addon_dir/ngx_http_casper_broker_cdn_public_module.cc"

NGX_ADDON_DEPS="$NGX_ADDON_DEPS \
$ngx_addon_dir/ngx_http_casper_broker_cdn_public_module.h \
$ngx_addon_dir/version.h"
