::
:: Build cpprestsdk 
::
::@echo off

set platform=%1
set build_type=%2
set force_shared_crt=%3

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if "%platform%"=="x64" (	
	set engine_dir=Program Files
	set generator="Visual Studio 14 2015 Win64"
)
if "%platform%"=="x86" (
	set engine_dir=Program Files (x86^)
	set generator="Visual Studio 14 2015"
)

set CPPRESTSDK_SOURCE_DIR=%scriptdir%\..\deps\cpprestsdk-2.10.4
if %ERRORLEVEL% NEQ 0 goto :error

set CPPRESTSDK_CMAKE_BUILD_DIR=%CPPRESTSDK_SOURCE_DIR%\cmake-build
if %ERRORLEVEL% NEQ 0 goto :error

set CPPRESTSDK_INSTALL_DIR=%scriptdir%\..\deps-build\%arcdir%\cpprestsdk\
if %ERRORLEVEL% NEQ 0 goto :error

if not exist %CPPRESTSDK_CMAKE_BUILD_DIR% mkdir %CPPRESTSDK_CMAKE_BUILD_DIR%
if %ERRORLEVEL% NEQ 0 goto :error

if not exist %CPPRESTSDK_INSTALL_DIR% mkdir %CPPRESTSDK_INSTALL_DIR%
if %ERRORLEVEL% NEQ 0 goto :error

cd %CPPRESTSDK_CMAKE_BUILD_DIR% 
if %ERRORLEVEL% NEQ 0 goto :error

REM https://github.com/aws/aws-sdk-cpp/issues/383
::set GIT_DIR=%TMP%

cmake %CPPRESTSDK_SOURCE_DIR% ^
-G %generator% ^
-DBOOST_ROOT=%scriptdir%\..\deps-build\%arcdir%\boost ^
-DBOOST_INCLUDEDIR=%scriptdir%\..\deps-build\%arcdir%\boost\include\boost-1_64 ^
-DBOOST_LIBRARYDIR=%scriptdir%\..\deps-build\%arcdir%\boost\lib ^
-DOPENSSL_ROOT_DIR=%scriptdir%\..\deps-build\%arcdir%\openssl ^
-DOPENSSL_LIBRARIES=%scriptdir%\..\deps-build\%arcdir%\openssl\lib ^
-DZLIB_LIBRARY=%scriptdir%\..\deps-build\%arcdir%\zlib\lib\zlib_a.lib ^
-DZLIB_INCLUDE_DIRS=%scriptdir%\..\deps-build\%arcdir%\zlib\include ^
-DBUILD_TESTS=OFF ^
-DBUILD_SHARED_LIBS=off ^
-DSTATIC_LINKING=on ^
-DBOOST_USE_STATIC_LIBS=on ^
-DCGAL_Boost_USE_STATIC_LIBS=true ^
-DBUILD_SAMPLES=off ^
-DCMAKE_INSTALL_PREFIX=%CPPRESTSDK_INSTALL_DIR%

if %ERRORLEVEL% NEQ 0 goto :error
	
msbuild INSTALL.vcxproj	/p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1