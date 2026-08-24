@echo off
setlocal
chcp 65001 >nul
title HodionKey - Cai dat

net session >nul 2>&1
if errorlevel 1 (
  echo [LOI] Can chay bang quyen Administrator.
  echo Bam chuot phai vao CaiDat.bat, chon "Run as administrator".
  pause
  exit /b 1
)

echo Thu muc cai dat: %~dp0
echo.
echo LUU Y: dang ky se ghi nho duong dan nay. Neu ban di chuyen hoac xoa
echo thu muc sau khi cai, bo go se hong. Hay dat thu muc o cho co dinh
echo (vi du C:\Program Files\HodionKey) TRUOC khi chay file nay.
echo.
pause

echo.
echo [1/2] Dang ky ban 64-bit...
regsvr32 /s "%~dp0x64\HodionKey.dll"
if errorlevel 1 (echo   ^-^> THAT BAI) else (echo   ^-^> OK)

echo [2/2] Dang ky ban 32-bit (cho ung dung 32-bit)...
if exist "%SystemRoot%\SysWOW64\regsvr32.exe" (
  "%SystemRoot%\SysWOW64\regsvr32.exe" /s "%~dp0x86\HodionKey.dll"
  if errorlevel 1 (echo   ^-^> THAT BAI) else (echo   ^-^> OK)
) else (
  echo   ^-^> Bo qua ^(Windows 32-bit^)
)

echo.
echo Xong. Buoc tiep theo:
echo   1. Settings ^> Time ^& language ^> Language ^& region
echo   2. Them ngon ngu "Tieng Viet" neu chua co
echo   3. Trong tuy chon ban phim cua Tieng Viet se thay "HodionKey"
echo   4. Chuyen ban phim bang Win+Space, bat/tat tieng Viet bang Ctrl+Space
echo.
echo   5. Chay x64\HodionKeyConfig.exe de co icon o khay he thong
echo      (bam trai de bat/tat tieng Viet, chuot phai de mo menu).
echo.
echo LUU Y: DUNG chay HodionKeyConfig.exe bang quyen Administrator.
echo Chay thuong thoi - neu khong cac ung dung dang go se khong noi
echo chuyen duoc voi no.
echo.
pause
