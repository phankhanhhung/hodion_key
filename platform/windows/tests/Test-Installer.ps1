<#
    Chạy thử bộ cài trên Windows THẬT (dùng trong CI).

    Đây là chỗ duy nhất kiểm được những thứ chỉ Windows mới có: regsvr32
    ghi đúng khoá gì, TSF nhận text service chưa, mục gỡ cài đặt có hiện
    trong Settings không, và gỡ xong có sạch không. Kiểm bằng cách CÀI THẬT
    rồi soi registry, không phải bằng cách đọc script.

        .\Test-Installer.ps1 -Package C:\pkg -InstallDir C:\HodionKeyTest

    Cần quyền Administrator. Mã thoát 0 = mọi phép kiểm đều qua.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Package,
    [string]$InstallDir = (Join-Path $env:TEMP 'HodionKeyTest')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# GUID phải khớp src/Guids.cpp. Chép tay ở đây là CỐ Ý: nếu ai đổi GUID
# trong C++ mà quên chỗ này thì test đỏ, đúng như mong muốn — đổi GUID của
# một text service đã phát hành là chuyện phải cân nhắc, không phải sửa
# lặng lẽ.
$Clsid = '{3FBE1B8E-9C52-4B7A-8E1D-5A642F0C917B}'
$ProfileGuid = '{A1E6F0D3-27C4-45B9-9B02-8F33D16E4A25}'
$LangId = '0x0000042a'   # LANG_VIETNAMESE
# GUID_TFCAT_TIP_KEYBOARD — không có category này thì Windows không coi
# text service là một bàn phím, và nó sẽ không hiện trong danh sách.
$CatKeyboard = '{34745C63-B2F0-4784-8B67-5E12C8701A31}'

$script:Passed = 0
$script:Failed = 0

function Check {
    param([string]$What, [bool]$Ok, [string]$Detail = '')
    if ($Ok) {
        Write-Host "  OK   $What"
        $script:Passed++
    }
    else {
        Write-Host "  HỎNG $What$(if ($Detail) { " — $Detail" })" -ForegroundColor Red
        $script:Failed++
    }
}

function Invoke-Script {
    param([string]$Path, [string[]]$Arguments)
    $all = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$Path`"") + $Arguments
    $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList $all -Wait -PassThru -NoNewWindow
    return $proc.ExitCode
}

function Get-RegDefault {
    param([string]$Path)
    try { return (Get-ItemProperty -LiteralPath $Path -Name '(default)' -ErrorAction Stop).'(default)' }
    catch { return $null }
}

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]$id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host 'Cần chạy bằng quyền Administrator.' -ForegroundColor Red
    exit 2
}

$installer = Join-Path $Package 'CaiDat.ps1'
$uninstaller = Join-Path $Package 'GoCaiDat.ps1'
foreach ($p in @($installer, $uninstaller)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Không thấy $p" }
}

Write-Host ''
Write-Host '=== 1. Cài (pha máy) ==='
$code = Invoke-Script -Path $installer -Arguments @(
    '-Phase', 'Machine', '-InstallDir', "`"$InstallDir`"", '-Quiet')
Check 'CaiDat.ps1 -Phase Machine trả về 0' ($code -eq 0) "mã $code"
if ($code -ne 0) {
    $log = Join-Path $env:TEMP 'HodionKey-CaiDat.log'
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | Write-Host }
}

Write-Host ''
Write-Host '=== 2. File đã nằm đúng chỗ ==='
foreach ($rel in @('x64\HodionKey.dll', 'x64\HodionKeyConfig.exe',
                   'x86\HodionKey.dll', 'x86\HodionKeyConfig.exe',
                   'GoCaiDat.ps1', 'GoCaiDat.bat', 'DOC-TRUOC-KHI-CAI.txt')) {
    Check "có $rel" (Test-Path -LiteralPath (Join-Path $InstallDir $rel))
}

Write-Host ''
Write-Host '=== 3. COM server đã đăng ký ==='
$inproc64 = Get-RegDefault "Registry::HKEY_CLASSES_ROOT\CLSID\$Clsid\InProcServer32"
Check 'HKCR\CLSID\...\InProcServer32 tồn tại' ($null -ne $inproc64)
if ($inproc64) {
    Check 'trỏ đúng vào DLL 64-bit vừa cài' `
        ($inproc64 -eq (Join-Path $InstallDir 'x64\HodionKey.dll')) $inproc64
    Check 'ThreadingModel = Apartment' `
        ((Get-ItemProperty -LiteralPath "Registry::HKEY_CLASSES_ROOT\CLSID\$Clsid\InProcServer32" `
            -Name 'ThreadingModel' -ErrorAction SilentlyContinue).ThreadingModel -eq 'Apartment')
}
$inproc32 = Get-RegDefault "Registry::HKEY_CLASSES_ROOT\WOW6432Node\CLSID\$Clsid\InProcServer32"
Check 'bản 32-bit đăng ký riêng dưới WOW6432Node' `
    ($inproc32 -eq (Join-Path $InstallDir 'x86\HodionKey.dll')) "$inproc32"

Write-Host ''
Write-Host '=== 4. TSF nhận text service ==='
$tipKey = "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$Clsid\LanguageProfile\$LangId\$ProfileGuid"
Check 'có language profile tiếng Việt' (Test-Path -LiteralPath $tipKey) $tipKey
if (Test-Path -LiteralPath $tipKey) {
    $desc = (Get-ItemProperty -LiteralPath $tipKey -Name 'Description' -ErrorAction SilentlyContinue)
    Check 'profile có mô tả' ($null -ne $desc -and -not [string]::IsNullOrWhiteSpace($desc.Description))
}
# Category KHÔNG nằm dưới HKCR\CLSID — ITfCategoryMgr ghi chúng vào cây
# CTF\TIP. Dò cả cây thay vì đoán đúng tầng, và in ra khi hỏng để lần sau
# không phải chạy lại CI mới biết nó nằm đâu.
$catRoot = "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$Clsid\Category"
$hasKeyboardCat = $false
if (Test-Path -LiteralPath $catRoot) {
    $hasKeyboardCat = @(Get-ChildItem -LiteralPath $catRoot -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*$CatKeyboard*" }).Count -gt 0
}
Check 'đã khai category bàn phím (GUID_TFCAT_TIP_KEYBOARD)' $hasKeyboardCat $catRoot
if (-not $hasKeyboardCat) {
    Write-Host '  --- có gì dưới CTF\TIP\<clsid> ---'
    Get-ChildItem -LiteralPath "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$Clsid" -Recurse `
        -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  $($_.Name)" }
}

Write-Host ''
Write-Host '=== 5. Mục gỡ cài đặt trong Settings > Apps ==='
$uninstallKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\HodionKey'
Check 'khoá gỡ cài đặt tồn tại' (Test-Path -LiteralPath $uninstallKey)
if (Test-Path -LiteralPath $uninstallKey) {
    $entry = Get-ItemProperty -LiteralPath $uninstallKey
    $exeVersion = (Get-Item -LiteralPath (Join-Path $InstallDir 'x64\HodionKeyConfig.exe')).VersionInfo.FileVersion
    Check 'DisplayName có' (-not [string]::IsNullOrWhiteSpace($entry.DisplayName))
    Check 'DisplayVersion khớp VERSIONINFO của exe' `
        ($entry.DisplayVersion -eq $exeVersion.Trim()) "$($entry.DisplayVersion) vs $exeVersion"
    Check 'InstallLocation đúng' ($entry.InstallLocation -eq $InstallDir)
    Check 'UninstallString trỏ vào script đã cài' `
        ($entry.UninstallString -like "*$(Join-Path $InstallDir 'GoCaiDat.ps1')*") $entry.UninstallString
}

Write-Host ''
Write-Host '=== 6. Pha người dùng ==='
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$code = Invoke-Script -Path $installer -Arguments @(
    '-Phase', 'User', '-InstallDir', "`"$InstallDir`"", '-NoLanguage', '-NoLaunch', '-Quiet')
Check 'CaiDat.ps1 -Phase User trả về 0' ($code -eq 0) "mã $code"
$run = (Get-ItemProperty -LiteralPath $runKey -Name 'HodionKey' -ErrorAction SilentlyContinue)
Check 'đã đặt khởi động cùng Windows' `
    ($null -ne $run -and $run.HodionKey -like "*HodionKeyConfig.exe*") "$($run.HodionKey)"

$code = Invoke-Script -Path $uninstaller -Arguments @(
    '-Phase', 'User', '-InstallDir', "`"$InstallDir`"", '-Quiet')
Check 'GoCaiDat.ps1 -Phase User trả về 0' ($code -eq 0) "mã $code"
Check 'đã bỏ mục khởi động cùng Windows' `
    ($null -eq (Get-ItemProperty -LiteralPath $runKey -Name 'HodionKey' -ErrorAction SilentlyContinue))

Write-Host ''
Write-Host '=== 7. Cài đè lên chính nó (nâng cấp) ==='
$code = Invoke-Script -Path $installer -Arguments @(
    '-Phase', 'Machine', '-InstallDir', "`"$InstallDir`"", '-Quiet')
Check 'cài lại lần hai vẫn trả về 0' ($code -eq 0) "mã $code"

Write-Host ''
Write-Host '=== 8. Gỡ (pha máy) ==='
$code = Invoke-Script -Path (Join-Path $InstallDir 'GoCaiDat.ps1') -Arguments @(
    '-Phase', 'Machine', '-InstallDir', "`"$InstallDir`"", '-Quiet')
Check 'GoCaiDat.ps1 -Phase Machine trả về 0' ($code -eq 0) "mã $code"
Check 'đã bỏ khoá CLSID' `
    (-not (Test-Path -LiteralPath "Registry::HKEY_CLASSES_ROOT\CLSID\$Clsid"))
Check 'đã bỏ language profile' (-not (Test-Path -LiteralPath $tipKey))
Check 'đã bỏ mục gỡ cài đặt' (-not (Test-Path -LiteralPath $uninstallKey))
Check 'đã xoá thư mục cài' (-not (Test-Path -LiteralPath $InstallDir))

Write-Host ''
Write-Host "Kết quả: $script:Passed qua, $script:Failed hỏng."
if ($script:Failed -gt 0) { exit 1 }
exit 0
