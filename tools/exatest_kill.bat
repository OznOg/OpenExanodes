@echo off

REM Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
REM reserved and protected by French, UK, U.S. and other countries' copyright laws.
REM This file is part of Exanodes project and is subject to the terms
REM and conditions defined in the LICENSE file which is present in the root
REM directory of the project.

setlocal

set REMOVE_CFG=false
set REMOVE_GOAL=false
set REMOVE_LOG=false

REM "tokens=2* %%i" means %%i contains the second token and %%j contains all the remaining tokens
for /F "tokens=2*" %%i in ('reg QUERY HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\exanodes_is1 /v InstallLocation') DO (
	set INSTALL_DIR="%%j"
)

echo Removing from %INSTALL_DIR%

:next_arg
if "%1"=="" goto no_more_arg
if "%1"=="/C" (
	set REMOVE_CFG=true
	shift
	goto next_arg
)	
if "%1"=="/G" (
	set REMOVE_GOAL=true
	shift
	goto next_arg
)
if "%1"=="/L" (
	set REMOVE_LOG=true
	shift
	goto next_arg
)

:usage
echo Kill Exanodes processes and unload its modules
echo Usage: %1 [/C] [/G] [/L] [/?]
echo   /C = also remove the config file (implies -g)
echo   /G = also remove the node goal file
echo   /L = also remove the log file
echo.
echo Beware that options /C, /G and /L are usually overkill and you better
echo know what you are doing.
goto exit

:no_more_arg

REM When removing the config file, also remove the goal file: no sense
REM in keeping it when there's no cluster configured
if "%REMOVE_CFG%"=="true" set REMOVE_GOAL=true

taskkill /F /IM exa_* 2> nul

if "%REMOVE_CFG%"=="true" (
    echo Removing config, hostname and license files
    del /Q %INSTALL_DIR%\cache\exanodes.conf 2> nul
    del /Q %INSTALL_DIR%\cache\hostname 2> nul
	del /Q %INSTALL_DIR%\cache\license 2> nul
)

if "%REMOVE_GOAL%"=="true" (
    echo Removing node goal file
    del /Q %INSTALL_DIR%\cache\node.goal 2> nul
)

if "%REMOVE_LOG%"=="true" (
    echo Removing log file
    del /Q %INSTALL_DIR%\log\exanodes.log 2> nul
)

tasklist /FI "IMAGENAME eq exa_*"

:exit

endlocal
