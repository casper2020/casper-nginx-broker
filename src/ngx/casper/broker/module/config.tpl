ngx_addon_name=ngx_http_casper_broker_module

HTTP_MODULES="$HTTP_MODULES ngx_http_casper_broker_module"

NGX_ADDON_SRCS="$NGX_ADDON_SRCS \
$ngx_addon_dir/ngx_http_casper_broker_module.cc"

NGX_ADDON_DEPS="$NGX_ADDON_DEPS \
$ngx_addon_dir/ngx_http_casper_broker_module.h \
$ngx_addon_dir/version.h"

CORE_LIBS="$CORE_LIBS -lstdc++ -lm"
CORE_LIBS+=" \
@OTHER_LIBS@ \
-L @PACKAGER_DIR@/../casper-nginx-broker/out/@PLATFORM@/@TARGET@ -lbroker \
-L @PACKAGER_DIR@/../casper-connectors/out/@PLATFORM@/@TARGET@ -lconnectors \
-L @PACKAGER_DIR@/../casper-osal/out/@PLATFORM@/@TARGET@ -losal \
-L @PACKAGER_DIR@/jsoncpp/out/@PLATFORM@/@TARGET@ -ljsoncpp \
-L @PACKAGER_DIR@/zlib/out/@PLATFORM@/@TARGET@ -lzlib \
-L @PACKAGER_DIR@/hiredis/out/@PLATFORM@/@TARGET@ -lhiredis \
-L @PACKAGER_DIR@/beanstalk-client/out/@PLATFORM@/@TARGET@ -lbeanstalkc \
-L @PACKAGER_DIR@/libbcrypt/out/@PLATFORM@/@TARGET@ -lbcrypt \
-L @LIBCURL_LIB_DIR@ -lcurl \
-L @ICU4C_LIB_DIR@ -licui18n \
-L @ICU4C_LIB_DIR@ -licuuc \
-L @OPENSSL_LIB_DIR@ -lssl \
-L @OPENSSL_LIB_DIR@ -lcrypto \
-L @POSTGRESQL_LIB_DIR@ -lpq \
-L @LIBEVENT2_LIB_DIR@ -levent \
-L @LIBEVENT2_LIB_DIR@ -levent_pthreads \
-L @LIBUNWIND_LIB_DIR@ -lunwind-x86_64 \
-L @LIBUNWIND_LIB_DIR@ -lunwind \
-llzma \
@SECURITY_FRAMEWORK@"
