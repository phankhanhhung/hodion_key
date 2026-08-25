<#
    HodionKey — gỡ cài đặt.

    Cũng hai pha như bộ cài, và cũng vì cùng một lý do: phần registry của
    NGƯỜI DÙNG (khởi động cùng Windows, cấu hình) phải gỡ dưới quyền người
    dùng, còn phần đăng ký DLL và xoá file thì cần quyền quản trị. Chạy tất
    bằng quyền admin sẽ để lại rác trong HKCU của người thật.

      .\GoCaiDat.ps1                # gỡ, giữ lại cấu hình
      .\GoCaiDat.ps1 -RemoveSettings   # gỡ và xoá luôn cấu hình
      .\GoCaiDat.ps1 -Quiet         # không hỏi, không chờ Enter

    Windows gọi đúng script này từ Settings > Apps.
    Mã thoát: 0 xong, 1 lỗi, 3010 xong nhưng cần khởi động lại.
#>
[CmdletBinding()]
param(
    [ValidateSet('All', 'Machine', 'User')]
    [string]$Phase = 'All',
    [string]$InstallDir,
    [string]$LogFile,
    [switch]$RemoveSettings,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$AppName = 'HodionKey'
$UninstallKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\HodionKey'
$RunKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$SettingsKey = 'HKCU:\Software\HodionKey'
$script:NeedReboot = $false

try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch { }

function Say {
    param([string]$Text = '')
    if (-not $Quiet) { Write-Host $Text }
    if ($LogFile) { Add-Content -LiteralPath $LogFile -Value $Text -Encoding UTF8 }
}

function Warn {
    param([string]$Text)
    Write-Warning $Text
    if ($LogFile) { Add-Content -LiteralPath $LogFile -Value "CANH BAO: $Text" -Encoding UTF8 }
}

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Resolve-InstallDir {
    if ($InstallDir) { return $InstallDir }
    $entry = Get-ItemProperty -Path $UninstallKey -Name 'InstallLocation' -ErrorAction SilentlyContinue
    if ($entry -and $entry.InstallLocation) { return $entry.InstallLocation }
    # Chạy thẳng script nằm trong thư mục cài thì chính chỗ đó là đáp án.
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'x64\HodionKey.dll')) { return $PSScriptRoot }
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'x86\HodionKey.dll')) { return $PSScriptRoot }
    return (Join-Path $env:ProgramFiles $AppName)
}

function Get-Regsvr32 {
    param([ValidateSet('x64', 'x86')][string]$Arch)
    if ($Arch -eq 'x64') {
        if (-not [Environment]::Is64BitOperatingSystem) { return $null }
        $dir = if ([Environment]::Is64BitProcess) { 'System32' } else { 'Sysnative' }
    }
    else {
        $dir = if ([Environment]::Is64BitProcess) { 'SysWOW64' } else { 'System32' }
    }
    $path = Join-Path $env:SystemRoot "$dir\regsvr32.exe"
    if (Test-Path -LiteralPath $path) { return $path }
    return $null
}

function Stop-Tray {
    $procs = @(Get-Process -Name 'HodionKeyConfig' -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return }
    foreach ($p in $procs) {
        try { $p.CloseMainWindow() | Out-Null } catch { }
    }
    Start-Sleep -Milliseconds 300
    foreach ($p in @(Get-Process -Name 'HodionKeyConfig' -ErrorAction SilentlyContinue)) {
        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch { }
    }
    Start-Sleep -Milliseconds 200
    Say '  Đã dừng tiến trình nền.'
}

$script:CanDelayDelete = $null
function Remove-FileAtReboot {
    param([string]$Path)
    if ($null -eq $script:CanDelayDelete) {
        try {
            Add-Type -Namespace HodionNativeDel -Name FileApi -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern bool MoveFileEx(string lpExistingFileName,
                                     string lpNewFileName, int dwFlags);
'@
            $script:CanDelayDelete = $true
        }
        catch { $script:CanDelayDelete = $false }
    }
    if (-not $script:CanDelayDelete) { return $false }
    # Đích null + MOVEFILE_DELAY_UNTIL_REBOOT = xoá lúc khởi động lại.
    return [HodionNativeDel.FileApi]::MoveFileEx($Path, $null, 0x4)
}

# ---------------------------------------------------------- pha NGƯỜI DÙNG --

function Uninstall-User {
    Stop-Tray
    if (Get-ItemProperty -Path $RunKey -Name $AppName -ErrorAction SilentlyContinue) {
        Remove-ItemProperty -Path $RunKey -Name $AppName -ErrorAction SilentlyContinue
        Say '  Đã bỏ mục khởi động cùng Windows.'
    }
    if ($RemoveSettings) {
        if (Test-Path -LiteralPath $SettingsKey) {
            Remove-Item -LiteralPath $SettingsKey -Recurse -Force -ErrorAction SilentlyContinue
            Say '  Đã xoá cấu hình người dùng.'
        }
    }
    else {
        Say "  Giữ lại cấu hình ở $SettingsKey (chạy lại với -RemoveSettings nếu muốn xoá)."
    }
}

# ---------------------------------------------------------------- pha MÁY --

function Uninstall-Machine {
    param([string]$Root)
    if (-not (Test-Admin)) { throw 'Pha máy cần quyền Administrator.' }

    Stop-Tray
    foreach ($arch in @('x64', 'x86')) {
        $dll = Join-Path $Root "$arch\HodionKey.dll"
        if (-not (Test-Path -LiteralPath $dll)) { continue }
        $exe = Get-Regsvr32 -Arch $arch
        if (-not $exe) { continue }
        $proc = Start-Process -FilePath $exe -ArgumentList @('/s', '/u', "`"$dll`"") `
            -Wait -PassThru -WindowStyle Hidden
        if ($proc.ExitCode -eq 0) { Say "  Đã gỡ đăng ký bản $arch." }
        else { Warn "Gỡ đăng ký $arch trả mã $($proc.ExitCode) — vẫn xoá file." }
    }

    if (Test-Path -LiteralPath $UninstallKey) {
        Remove-Item -LiteralPath $UninstallKey -Recurse -Force -ErrorAction SilentlyContinue
        Say '  Đã bỏ mục trong Settings > Apps.'
    }

    if (Test-Path -LiteralPath $Root) {
        # Đứng ra ngoài đã: script này nằm trong chính thư mục sắp xoá.
        Set-Location -LiteralPath $env:SystemRoot
        $stubborn = @()
        foreach ($file in @(Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue)) {
            try { Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop }
            catch { $stubborn += $file.FullName }
        }
        foreach ($path in $stubborn) {
            if (Remove-FileAtReboot -Path $path) { $script:NeedReboot = $true }
            else { Warn "Không xoá được $path — xoá tay sau." }
        }
        if ($stubborn.Count -eq 0) {
            Remove-Item -LiteralPath $Root -Recurse -Force -ErrorAction SilentlyContinue
            Say "  Đã xoá $Root."
        }
        else {
            Say "  Còn $($stubborn.Count) file đang được dùng — Windows sẽ xoá khi khởi động lại."
        }
    }
}

function Invoke-ElevatedMachinePhase {
    param([string]$Root)
    $log = Join-Path $env:TEMP 'HodionKey-GoCaiDat.log'
    Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue
    $self = Join-Path $PSScriptRoot 'GoCaiDat.ps1'
    $argList = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', "`"$self`"",
        '-Phase', 'Machine',
        '-InstallDir', "`"$Root`"",
        '-LogFile', "`"$log`""
    )
    if ($Quiet) { $argList += '-Quiet' }

    Say 'Xin quyền quản trị để gỡ đăng ký và xoá file…'
    try {
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList `
            -Verb RunAs -Wait -PassThru
    }
    catch {
        throw 'Bạn đã từ chối cửa sổ xin quyền — chưa gỡ xong.'
    }
    if (Test-Path -LiteralPath $log) {
        Get-Content -LiteralPath $log -Encoding UTF8 | ForEach-Object { Say $_ }
    }
    if ($proc.ExitCode -eq 3010) { $script:NeedReboot = $true }
    elseif ($proc.ExitCode -ne 0) {
        throw "Phần cần quyền quản trị thất bại (mã $($proc.ExitCode)). Xem $log."
    }
}

$exitCode = 0
try {
    $root = Resolve-InstallDir
    Say ''
    Say "=== $AppName — gỡ cài đặt ==="
    Say "  Thư mục: $root"
    Say ''
    switch ($Phase) {
        'Machine' { Uninstall-Machine -Root $root }
        'User' { Uninstall-User }
        'All' {
            Uninstall-User
            if (Test-Admin) { Uninstall-Machine -Root $root }
            else { Invoke-ElevatedMachinePhase -Root $root }
        }
    }
    if ($Phase -ne 'Machine') {
        Say ''
        Say 'Đã gỡ xong.'
        if ($script:NeedReboot) {
            Say 'Còn vài file đang được ứng dụng khác dùng; Windows sẽ dọn khi khởi động lại.'
        }
    }
    if ($script:NeedReboot) { $exitCode = 3010 }
}
catch {
    Write-Host ''
    Write-Host "LỖI: $($_.Exception.Message)" -ForegroundColor Red
    if ($LogFile) {
        Add-Content -LiteralPath $LogFile -Value "LOI: $($_.Exception.Message)" -Encoding UTF8
    }
    $exitCode = 1
}

if ($Phase -ne 'Machine' -and -not $Quiet) {
    Write-Host ''
    Write-Host 'Bấm Enter để đóng…' -NoNewline
    try { Read-Host | Out-Null } catch { }
}
exit $exitCode
