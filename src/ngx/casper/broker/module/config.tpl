ngx_addon_name=ngx_http_casper_broker_module

HTTP_MODULES="$HTTP_MODULES ngx_http_casper_broker_module"

NGX_ADDON_SRCS="$NGX_ADDON_SRCS \
$ngx_addon_dir/ngx_http_casper_broker_module.cc"

NGX_ADDON_DEPS="$NGX_ADDON_DEPS \
$ngx_addon_dir/ngx_http_casper_broker_module.h \
$ngx_addon_dir/version.h"

CORE_LIBS=" \
@OTHER_LIBS@ \
-L @PACKAGER_DIR@/build/@PLATFORM@/@PRJ_NAME@/@TARGET@/jsoncpp -ljsoncpp \
-L @PACKAGER_DIR@/build/@PLATFORM@/@PRJ_NAME@/@TARGET@/hiredis -lhiredis \
-L @PACKAGER_DIR@/build/@PLATFORM@/@PRJ_NAME@/@TARGET@/beanstalk-client -lbeanstalkc \
-L @PACKAGER_DIR@/build/@PLATFORM@/@PRJ_NAME@/@TARGET@/libbcrypt -lbcrypt \
-L @LIBCURL_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -lcurl @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @ZLIB_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -lz @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @ICU4C_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -licui18n @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @ICU4C_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -licuio @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @ICU4C_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -licuuc @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @ICU4C_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -licutu @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @ICU4C_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -licudata @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @POSTGRESQL_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -lpq @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @LIBEVENT2_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -levent @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @LIBEVENT2_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -levent_pthreads @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @LIBUNWIND_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -lunwind-x86_64 @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @LIBUNWIND_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -lunwind @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @LIBUUID_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -luuid @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-L @LIBMAGIC_LIB_DIR@ @PRJ_FORCE_STATIC_LIB_PREFERENCE@ -lmagic @PRJ_ALLOW_DYNAMIC_LIB_PREFERENCE@ \
-llzma \
@SECURITY_FRAMEWORK@"
CORE_LIBS+=" -lstdc++ -lm -ldl"
