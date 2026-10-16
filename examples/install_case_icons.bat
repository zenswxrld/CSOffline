@echo off
rem Copies your custom case pictures into the game so each custom case shows its own.
rem Name each picture after its case, e.g.  aurora.png  or  crate_csgogc_aurora.png  (transparent PNG, about 256 x 171).
rem Edit the two paths below, save, then double-click this file and restart the game twice.

set "GAME=G:\SteamLibrary\steamapps\common\csgo legacy"
set "SRC=C:\path\to\case_icons"

set "DEST=%GAME%\csgo\resource\flash\econ\weapon_cases"
set COUNT=0

if not exist "%GAME%\csgo" goto nogame
if not exist "%SRC%\*.png" goto nosrc
if not exist "%DEST%" mkdir "%DEST%"

for %%f in ("%SRC%\*.png") do call :copyone "%%f"

echo.
echo Copied %COUNT% picture(s) to %DEST%
echo Restart the game twice for them to show.
goto done

:copyone
set "NAME=%~n1"
echo %NAME%| findstr /b /i "crate_csgogc" >nul
if not errorlevel 1 goto plain
copy /Y "%~1" "%DEST%\crate_csgogc_%~nx1" >nul
goto counted
:plain
copy /Y "%~1" "%DEST%\%~nx1" >nul
:counted
set /a COUNT+=1
exit /b

:nogame
echo Could not find the game folder: %GAME%\csgo
echo Edit the GAME line at the top of this file.
goto done

:nosrc
echo Found no .png files in: %SRC%
echo Edit the SRC line at the top of this file.

:done
echo.
pause
