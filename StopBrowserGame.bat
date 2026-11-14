@echo off
rem Stops the hidden streamed game (only the -game instance, not your open Unreal Editor).
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { $_.CommandLine -match '-PixelStreamingConnectionURL' } | ForEach-Object { Stop-Process -Id $_.ProcessId }"
echo Streamed game stopped.
timeout /t 2 >nul
