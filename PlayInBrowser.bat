@echo off
rem Runs the Ricochet game with Pixel Streaming and opens it in your browser at http://localhost
setlocal
set "PROJECT_DIR=%~dp0"
set "UE_EXE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "SERVER_DIR=%PROJECT_DIR%PixelStreaming\SignallingWebServer\platform_scripts\cmd"

echo Starting Pixel Streaming web server (first run installs dependencies, can take a few minutes)...
start "Pixel Streaming Server - close this window to stop" cmd /c ""%SERVER_DIR%\start.bat""

echo Waiting for the web server on http://localhost ...
:wait_server
timeout /t 2 /nobreak >nul
curl -s -o nul http://127.0.0.1/ || goto wait_server

echo Starting the game (runs hidden, streamed to the browser)...
start "Ricochet" "%UE_EXE%" "%PROJECT_DIR%MyProject.uproject" /Game/L_Room01 -game -RenderOffScreen -ResX=1920 -ResY=1080 -ForceRes -AudioMixer -PixelStreamingConnectionURL=ws://127.0.0.1:8888 -log

timeout /t 10 /nobreak >nul
start "" http://localhost/

echo.
echo Game is streaming at http://localhost  - click "Click to start" in the browser.
echo To stop: close the "Pixel Streaming Server" window and run StopBrowserGame.bat
pause
