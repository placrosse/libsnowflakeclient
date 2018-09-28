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

::BUILD BOOST
if not exist "%scriptdir%\..\deps-build\%arcdir%\boost\" (
  call "%scriptdir%\build_boost.bat" %platform% %build_type% %dynamic_runtime%
  if %ERRORLEVEL% NEQ 0 goto :error
)

::BUILD CPPRESTSDK
if not exist "%scriptdir%\..\deps-build\%arcdir%\cpprestsdk\" (
  call "%scriptdir%\build_cpprestsdk.bat" %platform% %build_type% %dynamic_runtime%
  if %ERRORLEVEL% NEQ 0 goto :error
)

set AZURESTORAGECPP_SOURCE_DIR=%scriptdir%\..\deps\azure-storage-cpp-5.1.1
if %ERRORLEVEL% NEQ 0 goto :error

::Download source code of azure storage cpp
if not exist "%AZURESTORAGECPP_SOURCE_DIR%" (
	cd %scriptdir%\..\deps

	curl -L https://github.com/howryu/azure-storage-cpp/archive/v5.1.1.zip > azure-storage-cpp-5.1.1.zip
	if %ERRORLEVEL% NEQ 0 goto :error
	
	unzip azure-storage-cpp-5.1.1.zip
	if %ERRORLEVEL% NEQ 0 goto :error
)


set AZURESTORAGECPP_CMAKE_BUILD_DIR=%AZURESTORAGECPP_SOURCE_DIR%\cmake-build
if %ERRORLEVEL% NEQ 0 goto :error

set AZURESTORAGECPP_INSTALL_DIR=%scriptdir%\..\deps-build\%arcdir%\azure_storage_cpp\
if %ERRORLEVEL% NEQ 0 goto :error

if not exist %AZURESTORAGECPP_CMAKE_BUILD_DIR% mkdir %AZURESTORAGECPP_CMAKE_BUILD_DIR%
if %ERRORLEVEL% NEQ 0 goto :error

if not exist %AZURESTORAGECPP_INSTALL_DIR% mkdir %AZURESTORAGECPP_INSTALL_DIR%
if %ERRORLEVEL% NEQ 0 goto :error

cd %AZURESTORAGECPP_CMAKE_BUILD_DIR% 
if %ERRORLEVEL% NEQ 0 goto :error


REM https://github.com/aws/aws-sdk-cpp/issues/383
::set GIT_DIR=%TMP%

cmake %AZURESTORAGECPP_SOURCE_DIR%\Microsoft.WindowsAzure.Storage ^
-G %generator% ^
-DBOOST_ROOT=%scriptdir%\..\deps-build\%arcdir%\boost ^
-DBOOST_INCLUDEDIR=%scriptdir%\..\deps-build\%arcdir%\boost\include\boost-1_64 ^
-DBOOST_LIBRARYDIR=%scriptdir%\..\deps-build\%arcdir%\boost\lib ^
-DOPENSSL_ROOT_DIR=%scriptdir%\..\deps-build\%arcdir%\openssl ^
-DOPENSSL_LIBRARIES=%scriptdir%\..\deps-build\%arcdir%\openssl\lib ^
-DZLIB_LIBRARY=%scriptdir%\..\deps-build\%arcdir%\zlib\lib\zlib_a.lib ^
-DZLIB_INCLUDE_DIRS=%scriptdir%\..\deps-build\%arcdir%\zlib\include ^
-DCASABLANCA_INCLUDE_DIR=%scriptdir%\..\deps-build\%arcdir%\cpprestsdk\include ^
-DCASABLANCA_LIBRARY=%scriptdir%\..\deps-build\%arcdir%\cpprestsdk\lib ^
-DBUILD_TESTS=OFF ^
-DBUILD_SHARED_LIBS=off ^
-DSTATIC_LINKING=on ^
-DBOOST_USE_STATIC_LIBS=on ^
-DCGAL_Boost_USE_STATIC_LIBS=true ^
-DBUILD_SAMPLES=off ^
-DCMAKE_INSTALL_PREFIX=%AZURESTORAGECPP_INSTALL_DIR%


if %ERRORLEVEL% NEQ 0 goto :error
	
msbuild INSTALL.vcxproj	/p:Configuration=%build_type%
if %ERRORLEVEL% NEQ 0 goto :error

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1