#!/bin/bash -e
#
# build microsoft cpprestsdk
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build Microsoft cpprestsdk"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
CPPRESTSDK_SOURCE_DIR=$DEPS_DIR/cpprestsdk-2.10.1/
CPPRESTSDK_CMAKE_BUILD_DIR=$CPPRESTSDK_SOURCE_DIR/cmake-build
CPPRESTSDK_INSTALL_DIR=$DEPENDENCY_DIR/azure

cpprestsdk_configure_opts=()
if [[ "$target" != "Release" ]]; then
    cpprestsdk_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
else
    cpprestsdk_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
cpprestsdk_configure_opts+=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DCMAKE_INSTALL_PREFIX=$CPPRESTSDK_INSTALL_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DCMAKE_PREFIX_PATH=\"$LIBCURL_BUILD_DIR/;$OPENSSL_BUILD_DIR/\""
    "-DOPENSSL_ROOT_DIR=$DEPENDENCY_DIR/openssl"
    "-DOPENSSL_USE_STATIC_LIBS=true"
    "-DBOOST_ROOT=/opt/boost"
    "-DWERROR=OFF"
    "-DBUILD_TESTS=OFF"
)

rm -rf $CPPRESTSDK_INSTALL_DIR
rm -rf $CPPRESTSDK_CMAKE_BUILD_DIR
mkdir $CPPRESTSDK_INSTALL_DIR
mkdir $CPPRESTSDK_CMAKE_BUILD_DIR

cd $CPPRESTSDK_CMAKE_BUILD_DIR
$CMAKE ${cpprestsdk_configure_opts[@]} ../Release
make
make install
