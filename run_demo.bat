@echo off
title CM TRADING SYSTEM - ORDER CONSOLE (PLACE ORDERS HERE)
echo =======================================================================
echo   Starting C++ FIX Acceptor Server (Minimized)...
echo   (Logs will run in a separate minimized window: FIX EXCHANGE SERVER)
echo =======================================================================
start /min "FIX EXCHANGE SERVER (LOGS ONLY)" "build_msvc\Release\exchange_fix.exe"

:: Wait for the server to initialize
timeout /t 2 /nobreak > nul

echo.
echo =======================================================================
echo   Running Interactive TUI Client...
echo   *** CRITICAL: PLACE ALL ORDERS IN THIS WINDOW! ***
echo =======================================================================
python apps/fix_tui_client.py

echo.
echo =======================================================================
echo   Demo run completed successfully!
echo =======================================================================
pause
