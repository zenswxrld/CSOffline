@echo off
rem Gives you stars while the game is running. Double-click, type an amount, press Enter.
rem One-time setup: in csgo_gc\config.txt set  "rcon" { "enabled" "1" ... }  and restart the game.
rem Keep this file in the same folder as rcon.ps1.

set /p AMOUNT=How many stars (1 to 1000000)? 
if "%AMOUNT%"=="" set AMOUNT=1000

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0rcon.ps1" "give_stars %AMOUNT%"

echo.
echo If it says the connection was refused, RCON is off or the game is not running.
pause
