echo off

REM Copy the .dll to all game directories
REM This is easier way than using the MSVC custom build option for
REM all commands

set CODEDIR="."
set STEAMDIR="C:\Program Files (x86)\Steam\steamapps\common\Half-Life"

echo on

copy %CODEDIR%\Debug\marine_bot.dll     %STEAMDIR%\dod\marinebot_dod
