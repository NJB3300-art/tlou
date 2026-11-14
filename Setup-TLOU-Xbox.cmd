@echo off
rem The Last of Us Remastered for the Xbox app (UWP, Developer Mode): one-click setup.
rem Finds your PS4 dump (CUSA00552 + update 1.11) on this PC, prepares it, then sends it to the
rem Xbox over Wi-Fi (in the app: Receive files over Wi-Fi) or copies it to a USB drive.
rem Run it again after an interruption: it continues where it stopped.
cd /d "%~dp0"
python tools\xbox_setup.py %*
pause
