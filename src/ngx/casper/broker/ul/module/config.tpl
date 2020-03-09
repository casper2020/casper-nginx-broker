ngx_addon_name=ngx_http_casper_broker_ul_module

HTTP_MODULES="$HTTP_MODULES ngx_http_casper_broker_ul_module"

NGX_ADDON_SRCS="$NGX_ADDON_SRCS \
$ngx_addon_dir/ngx_http_casper_broker_ul_module.cc"

NGX_ADDON_DEPS="$NGX_ADDON_DEPS \
$ngx_addon_dir/ngx_http_casper_broker_ul_module.h \
$ngx_addon_dir/version.h"

CORE_LIBS="$CORE_LIBS -lstdc++ -lm"
CORE_LIBS+=" \
-L @PACKAGER_DIR@/jsoncpp/out/@PLATFORM@/@TARGET@ -ljsoncpp \
-L @PACKAGER_DIR@/zlib/out/@PLATFORM@/@TARGET@ -lzlib \
-L @PACKAGER_DIR@/hiredis/out/@PLATFORM@/@TARGET@ -lhiredis \
-L @PACKAGER_DIR@/beanstalk-client/out/@PLATFORM@/@TARGET@ -lbeanstalkc \
-L @PACKAGER_DIR@/libbcrypt/out/@PLATFORM@/@TARGET@ -lbcrypt \
-L @PACKAGER_DIR@/beanstalk-client/out/@PLATFORM@/@TARGET@ \
-L @PACKAGER_DIR@/curl/@PLATFORM@/pkg/@TARGET@/curl/usr/local/casper/curl/lib -lcurl \
-L @PACKAGER_DIR@/../casper-osal/out/@PLATFORM@/@TARGET@ -losal \
-L @PACKAGER_DIR@/../casper-connectors/out/@PLATFORM@/@TARGET@ -lconnectors \
-L @PACKAGER_DIR@/../casper-nginx-broker/out/@PLATFORM@/@TARGET@ -lbroker \
-L @ICU4C_LIB_DIR@ -licuuc -licui18n \
-L @OPENSSL_LIB_DIR@ -lssl -lcrypto \
-L @POSTGRESQL_LIB_DIR@ -lpq \
-levent \
-levent_pthreads \
@SECURITY_FRAMEWORK@"

