@echo off
rem Chi la vo boc cho CaiDat.ps1 de bam dup chay duoc.
rem KHONG can chay bang quyen Administrator: script tu xin quyen cho dung
rem phan can quyen, phan con lai chay duoi quyen nguoi dung (quan trong -
rem tien trinh nen ma chay quyen admin thi cac ung dung khac khong noi
rem duoc vao no).
setlocal
chcp 65001 >nul
title HodionKey - Cai dat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0CaiDat.ps1" %*
exit /b %ERRORLEVEL%
