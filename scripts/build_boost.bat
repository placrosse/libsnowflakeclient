::
:: Build boost 
::
::@echo off

set platform=%1
set build_type=%2
set use_shared_runtime_link=%3

set scriptdir=%~dp0
call "%scriptdir%\_init.bat" %platform% %build_type%
if %ERRORLEVEL% NEQ 0 goto :error
set curdir=%cd%

if "%platform%"=="x64" (	
	set engine_dir=Program Files
	set generator="Visual Studio 14 2015 Win64"
	set address_model=64
)
if "%platform%"=="x86" (
	set engine_dir=Program Files (x86^)
	set generator="Visual Studio 14 2015"
	set address_model=32
)

if "%build_type%"=="Release" (
    set variant=release
)
if "%build_type%"=="Debug" (
    set variant=debug
)

set runtime_link=static
if "%use_shared_runtime_link%"=="true" (
	set runtime_link=shared
)


set BOOST_SOURCE_DIR=%scriptdir%\..\deps\boost_1_64_0\
set BOOST_INSTALL_DIR=%scriptdir%\..\deps-build\%arcdir%\boost\
SET OPENSSL_ROOT_DIR=%scriptdir%\..\deps-build\%arcdir%\openssl\

if exist %BOOST_INSTALL_DIR% (
  goto :success
)

:: DOWNLOAD BOOST SOURCE CODE IF NOT EXISTS
if NOT EXIST %BOOST_SOURCE_DIR% (
  cd %scriptdir%\..\deps
  curl -L https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.zip > boost_1_64_0.zip
  if %ERRORLEVEL% NEQ 0 goto :error
  
  unzip boost_1_64_0.zip
  if %ERRORLEVEL% NEQ 0 goto :error
)

cd %BOOST_SOURCE_DIR%

call bootstrap.bat 
b2 link=static ^
toolset=msvc-14.0 ^
variant=%variant% ^
address-model=%address_model% ^
threading=multi ^
runtime-link=%runtime_link% ^
--prefix=%BOOST_INSTALL_DIR% ^
--with-system ^
--with-regex ^
--with-date_time ^
install

:success
cd "%curdir%"
exit /b 0

:error
cd "%curdir%"
exit /b 1
