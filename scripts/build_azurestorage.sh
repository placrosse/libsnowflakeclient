#!/bin/bash -e
#
# build azure storage sdk
#
function usage() {
    echo "Usage: `basename $0` [-t <Release|Debug>]"
    echo "Build azure storage sdk"
    echo "-t <Release/Debug> : Release or Debug builds"
    exit 2
}
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/_init.sh
AZURE_STORAGE_SOURCE_DIR=$DEPS_DIR/azure-storage-cpp-5.0.0/
AZURE_STORAGE_CMAKE_BUILD_DIR=$AZURE_STORAGE_SOURCE_DIR/cmake-build
AZURE_STORAGE_INSTALL_DIR=$DEPENDENCY_DIR/azure-storage

azure_storage_configure_opts=()
if [[ "$target" != "Release" ]]; then
    azure_storage_configure_opts+=("-DCMAKE_BUILD_TYPE=Debug")
else
    azure_storage_configure_opts+=("-DCMAKE_BUILD_TYPE=Release")
fi
azure_storage_configure_opts+=(
    "-DCMAKE_C_COMPILER=$GCC"
    "-DCMAKE_CXX_COMPILER=$GXX"
    "-DCMAKE_INSTALL_PREFIX=$AZURE_STORAGE_INSTALL_DIR"
    "-DBUILD_SHARED_LIBS=OFF"
#    "-DOPENSSL_ROOT_DIR=$DEPENDENCY_DIR/openssl"
#    "-DOPENSSL_USE_STATIC_LIBS=true"
    "-DBOOST_ROOT=/opt/boost_1_67_0"
    "-DBUILD_TESTS=OFF"
    "-DBUILD_SAMPLES=OFF"
)

rm -rf $AZURE_STORAGE_INSTALL_DIR
rm -rf $AZURE_STORAGE_CMAKE_BUILD_DIR
mkdir $AZURE_STORAGE_INSTALL_DIR
mkdir $AZURE_STORAGE_CMAKE_BUILD_DIR

cd $AZURE_STORAGE_CMAKE_BUILD_DIR
CASABLANCA_DIR=$DEPENDENCY_DIR/cpprestsdk $CMAKE ${azure_storage_configure_opts[@]} ../Microsoft.WindowsAzure.Storage
make
make install
