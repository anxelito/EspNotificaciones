@echo off
rem Installs the panel now-playing agent on this PC (needs Python 3.10+ in PATH).
cd /d "%~dp0"
where python >nul 2>nul || (echo Python not found. Install it from python.org ^(tick "Add to PATH"^) and run this again. & pause & exit /b 1)
if not exist config.json (echo Missing config.json: copy config.example.json to config.json and fill api_key. & pause & exit /b 1)
python -m pip install --user -r requirements.txt || (pause & exit /b 1)
python nowplaying_agent.py --install
start "" pythonw "%~dp0nowplaying_agent.py"
echo.
echo Agent running in background. Log: %~dp0agent.log
pause
