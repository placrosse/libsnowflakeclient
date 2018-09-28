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

if not exist "%CPPRESTSDK_SOURCE_DIR%" (
  cd %scriptdir%\..\deps

  curl -L https://github.com/howryu/cpprestsdk/archive/v2.10.4.zip > cpprestsdk-2.10.4.zip
  if %ERRORLEVEL% NEQ 0 goto :error
  
  unzip cpprestsdk-2.10.4
  if %ERRORLEVEL% NEQ 0 goto :error
) 


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

:: rename openssl lib
copy /y "%scriptdir%\..\deps-build\%arcdir%\openssl\lib\libcrypto_a.lib" ^
"%scriptdir%\..\deps-build\%arcdir%\openssl\lib\libcrypto.lib"
if %ERRORLEVEL% NEQ 0 goto :error

copy /y "%scriptdir%\..\deps-build\%arcdir%\openssl\lib\libssl_a.lib" ^
"%scriptdir%\..\deps-build\%arcdir%\openssl\lib\libssl.lib"
if %ERRORLEVEL% NEQ 0 goto :error

:: rename boost lib
copy /y "%scriptdir%\..\deps-build\%arcdir%\boost\lib\libboost_date_time-vc140-mt-s-1_64.lib" ^
"%scriptdir%\..\deps-build\%arcdir%\boost\lib\boost_date_time.lib"
if %ERRORLEVEL% NEQ 0 goto :error

copy /y "%scriptdir%\..\deps-build\%arcdir%\boost\lib\libboost_regex-vc140-mt-s-1_64.lib" ^
"%scriptdir%\..\deps-build\%arcdir%\boost\lib\boost_regex.lib"
if %ERRORLEVEL% NEQ 0 goto :error

copy /y "%scriptdir%\..\deps-build\%arcdir%\boost\lib\libboost_system-vc140-mt-s-1_64.lib" ^
"%scriptdir%\..\deps-build\%arcdir%\boost\lib\boost_system.lib"
if %ERRORLEVEL% NEQ 0 goto :error

:: rename zlib
copy /y "%scriptdir%\..\deps-build\%arcdir%\zlib\lib\zlib_a.lib" ^
"%scriptdir%\..\deps-build\%arcdir%\zlib\lib\zlib.lib"
if %ERRORLEVEL% NEQ 0 goto :error


cmake %CPPRESTSDK_SOURCE_DIR% ^
-G %generator% ^
-DBOOST_ROOT=%scriptdir%\..\deps-build\%arcdir%\boost ^
-DBOOST_INCLUDEDIR=%scriptdir%\..\deps-build\%arcdir%\boost\include\boost-1_64 ^
-DBOOST_LIBRARYDIR=%scriptdir%\..\deps-build\%arcdir%\boost\lib ^
-DOPENSSL_ROOT_DIR=%scriptdir%\..\deps-build\%arcdir%\openssl ^
-DOPENSSL_LIBRARIES=%scriptdir%\..\deps-build\%arcdir%\openssl\lib ^
-DZLIB_LIBRARY=%scriptdir%\..\deps-build\%arcdir%\zlib\lib\ ^
-DZLIB_INCLUDE_DIR=%scriptdir%\..\deps-build\%arcdir%\zlib\include ^
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