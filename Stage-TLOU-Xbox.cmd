@echo off
rem The Last of Us Remastered (CUSA00552) for the UWP app (Xbox Developer Mode): prepares the game
rem here on the PC and writes uwp-stage\ (boot.json and the prepared files). Copy uwp-stage to the
rem console as TLOU\stage, next to TLOU\CUSA00552 and TLOU\CUSA00552-patch.
rem The game folder is ..\CUSA00552 (or --game <folder>); the resolution is the launcher's (tlou.ini).
cd /d "%~dp0"
python tools\probe_tlou.py --stage "%~dp0uwp-stage" %*
if errorlevel 1 pause
