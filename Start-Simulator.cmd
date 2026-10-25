@echo off
setlocal
cd /d "%~dp0"
set PYTHONUTF8=1
".venv\Scripts\python.exe" scripts_local\simulator_web.py %*
if errorlevel 1 pause
