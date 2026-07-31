@echo off
rem SHITFIRE localhost launcher — mic permission persists on this origin.
rem Double-click me. Ctrl+C in this window stops the server.
cd /d "%~dp0"
where node >nul 2>nul
if %errorlevel%==0 (
  node serve.js
) else if exist "C:\Program Files\nodejs\node.exe" (
  "C:\Program Files\nodejs\node.exe" serve.js
) else (
  echo Node.js not found - install Node, or keep double-clicking SHITFIRE.html
  echo directly ^(everything works there too; the mic just asks once per session^).
  pause
)
