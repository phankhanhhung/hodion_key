@echo off
setlocal
chcp 65001 >nul
title HodionKey - Go cai dat

net session >nul 2>&1
if errorlevel 1 (
  echo [LOI] Can chay bang quyen Administrator.
  pause
  exit /b 1
)

echo Dang go ban 64-bit...
regsvr32 /s /u "%~dp0x64\HodionKey.dll"
if exist "%SystemRoot%\SysWOW64\regsvr32.exe" (
  echo Dang go ban 32-bit...
  "%SystemRoot%\SysWOW64\regsvr32.exe" /s /u "%~dp0x86\HodionKey.dll"
)
echo Dang dong tien trinh nen va bo muc khoi dong cung Windows...
taskkill /im HodionKeyConfig.exe /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v HodionKey /f >nul 2>&1

echo.
echo Da go. Cau hinh van con o HKCU\Software\HodionKey, xoa tay neu muon.
pause
