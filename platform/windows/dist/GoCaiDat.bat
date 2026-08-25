@echo off
rem Vo boc cho GoCaiDat.ps1. Xem chu thich trong CaiDat.bat.
setlocal
chcp 65001 >nul
title HodionKey - Go cai dat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0GoCaiDat.ps1" %*
exit /b %ERRORLEVEL%
