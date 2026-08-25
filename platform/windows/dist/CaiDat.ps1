<#
    HodionKey — bộ cài.

    Chạy hai pha, và đó là điểm mấu chốt chứ không phải chi tiết vặt:

      * pha MÁY  — chép file vào Program Files, đăng ký DLL với Windows,
                   ghi mục gỡ cài đặt. Cần quyền Administrator.
      * pha NGƯỜI DÙNG — thêm tiếng Việt vào danh sách ngôn ngữ, đặt khởi
                   động cùng Windows, bật tiến trình nền. KHÔNG được chạy
                   bằng quyền admin: tiến trình nền chạy ở mức toàn vẹn cao
                   thì các ứng dụng thường sẽ không nối được vào named pipe
                   của nó, và phần đoán dấu im lặng biến mất.

    Cách dùng thường ngày: bấm đúp CaiDat.bat. Script tự xin quyền cho đúng
    phần cần quyền, phần còn lại chạy dưới quyền người dùng.

      .\CaiDat.ps1                      # cài đầy đủ
      .\CaiDat.ps1 -InstallDir D:\HK    # chọn chỗ cài
      .\CaiDat.ps1 -NoLanguage -NoLaunch -NoAutoStart -Quiet
      .\CaiDat.ps1 -Phase Machine       # chỉ phần cần quyền (bộ cài tự gọi)

    Mã thoát: 0 xong, 1 lỗi, 3010 xong nhưng cần khởi động lại.
#>
[CmdletBinding()]
param(
    [ValidateSet('All', 'Machine', 'User')]
    [string]$Phase = 'All',
    [string]$InstallDir,
    [string]$LogFile,
    [switch]$NoLanguage,
    [switch]$NoAutoStart,
    [switch]$NoLaunch,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$AppName = 'HodionKey'
$UninstallKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\HodionKey'
$RunKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$Source = $PSScriptRoot
$script:NeedReboot = $false

if (-not $InstallDir) {
    $InstallDir = Join-Path $env:ProgramFiles $AppName
}

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

# regsvr32 đúng bit. Trên tiến trình 32-bit của Windows 64-bit, System32 bị
# chuyển hướng sang SysWOW64, nên muốn với tới bản 64-bit phải đi qua
# Sysnative — đây là chỗ hay sai và sai thì lặng lẽ đăng ký nhầm bit.
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

function Invoke-Regsvr32 {
    param([string]$Exe, [string]$Dll, [switch]$Unregister)
    $argList = @('/s')
    if ($Unregister) { $argList += '/u' }
    $argList += "`"$Dll`""
    $proc = Start-Process -FilePath $Exe -ArgumentList $argList -Wait -PassThru -WindowStyle Hidden
    return $proc.ExitCode
}

function Stop-Tray {
    $procs = @(Get-Process -Name 'HodionKeyConfig' -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return }
    Say "  Dừng tiến trình nền đang chạy ($($procs.Count))…"
    foreach ($p in $procs) {
        try { $p.CloseMainWindow() | Out-Null } catch { }
    }
    Start-Sleep -Milliseconds 300
    foreach ($p in @(Get-Process -Name 'HodionKeyConfig' -ErrorAction SilentlyContinue)) {
        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch { }
    }
    Start-Sleep -Milliseconds 200
}

# Thay file kể cả khi nó đang bị nạp trong tiến trình khác. HodionKey.dll
# nằm trong MỌI ứng dụng đang gõ, nên nâng cấp mà không xử chuyện này thì
# gần như chắc chắn hỏng.
$script:CanDelayReplace = $null
function Move-FileAtReboot {
    param([string]$From, [string]$To)
    if ($null -eq $script:CanDelayReplace) {
        try {
            Add-Type -Namespace HodionNative -Name FileApi -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern bool MoveFileEx(string lpExistingFileName,
                                     string lpNewFileName, int dwFlags);
'@
            $script:CanDelayReplace = $true
        }
        catch { $script:CanDelayReplace = $false }
    }
    if (-not $script:CanDelayReplace) { return $false }
    # MOVEFILE_REPLACE_EXISTING | MOVEFILE_DELAY_UNTIL_REBOOT
    return [HodionNative.FileApi]::MoveFileEx($From, $To, 0x1 -bor 0x4)
}

function Copy-Payload {
    param([string]$From, [string]$To)
    try {
        Copy-Item -LiteralPath $From -Destination $To -Force
        return
    }
    catch {
        if (-not (Test-Path -LiteralPath $To)) { throw }
    }
    # File đang bị khoá: đặt bản mới cạnh nó rồi hẹn Windows tráo lúc khởi
    # động lại. Bản đang chạy vẫn dùng được cho tới lúc đó.
    $staged = "$To.moi"
    Copy-Item -LiteralPath $From -Destination $staged -Force
    if (Move-FileAtReboot -From $staged -To $To) {
        $script:NeedReboot = $true
        Say "  $([IO.Path]::GetFileName($To)) đang được dùng — sẽ thay khi khởi động lại."
    }
    else {
        Remove-Item -LiteralPath $staged -Force -ErrorAction SilentlyContinue
        throw "Không ghi đè được $To (file đang được dùng). Hãy đăng xuất rồi chạy lại."
    }
}

function Get-SourceArch {
    $found = @()
    foreach ($arch in @('x64', 'x86')) {
        if (Test-Path -LiteralPath (Join-Path $Source "$arch\HodionKey.dll")) {
            $found += $arch
        }
    }
    return $found
}

function Get-FileVersionString {
    param([string]$Path)
    try {
        $v = (Get-Item -LiteralPath $Path).VersionInfo.FileVersion
        if ($v) { return $v.Trim() }
    }
    catch { }
    return '0.0.0'
}

# ---------------------------------------------------------------- pha MÁY --

function Install-Machine {
    if (-not (Test-Admin)) { throw 'Pha máy cần quyền Administrator.' }

    $arches = Get-SourceArch
    if ($arches.Count -eq 0) {
        throw "Không thấy x64\HodionKey.dll hay x86\HodionKey.dll cạnh CaiDat.bat. Giải nén CẢ gói ra một thư mục rồi chạy lại."
    }
    Say "Cài $AppName vào: $InstallDir"
    Say "  Kiến trúc có trong gói: $($arches -join ', ')"

    Stop-Tray

    # Bản cũ ở chỗ KHÁC thì gỡ đăng ký nó trước, nếu không Windows còn giữ
    # một text service trỏ vào file không còn tồn tại.
    $old = Get-ItemProperty -Path $UninstallKey -Name 'InstallLocation' -ErrorAction SilentlyContinue
    if ($old -and $old.InstallLocation -and
        $old.InstallLocation.TrimEnd('\') -ne $InstallDir.TrimEnd('\')) {
        Say "  Gỡ đăng ký bản cũ ở $($old.InstallLocation)…"
        Unregister-Dll -Root $old.InstallLocation
    }

    foreach ($arch in $arches) {
        $dest = Join-Path $InstallDir $arch
        New-Item -ItemType Directory -Path $dest -Force | Out-Null
        foreach ($name in @('HodionKey.dll', 'HodionKeyConfig.exe')) {
            $src = Join-Path $Source "$arch\$name"
            if (Test-Path -LiteralPath $src) {
                Copy-Payload -From $src -To (Join-Path $dest $name)
            }
        }
    }

    # Tài liệu, script gỡ cài đặt và (nếu có) dữ liệu ngôn ngữ. Dữ liệu
    # không nằm trong repo vì giấy phép, nên có thì chép, không thì thôi —
    # thiếu nó chỉ mất phần đoán dấu, gõ vẫn nguyên vẹn.
    foreach ($name in @('DOC-TRUOC-KHI-CAI.txt', 'GIAY-PHEP-TU-DIEN.txt',
                        'CaiDat.ps1', 'CaiDat.bat',
                        'GoCaiDat.ps1', 'GoCaiDat.bat')) {
        $src = Join-Path $Source $name
        if (Test-Path -LiteralPath $src) {
            Copy-Payload -From $src -To (Join-Path $InstallDir $name)
        }
    }
    $dataCopied = 0
    foreach ($name in @('viet-syllables.txt', 'viet-ngram.bin')) {
        $src = Join-Path $Source $name
        if (Test-Path -LiteralPath $src) {
            foreach ($arch in $arches) {
                Copy-Payload -From $src -To (Join-Path (Join-Path $InstallDir $arch) $name)
            }
            $dataCopied++
        }
    }
    if ($dataCopied -eq 0) {
        Say '  (Không có dữ liệu đoán dấu trong gói — xem DOC-TRUOC-KHI-CAI.txt.)'
    }

    Register-Dll -Root $InstallDir -Arches $arches
    Write-UninstallEntry -Arches $arches
    Say '  Xong phần cần quyền quản trị.'
}

function Register-Dll {
    param([string]$Root, [string[]]$Arches)
    $done = 0
    foreach ($arch in $Arches) {
        $dll = Join-Path $Root "$arch\HodionKey.dll"
        if (-not (Test-Path -LiteralPath $dll)) { continue }
        $exe = Get-Regsvr32 -Arch $arch
        if (-not $exe) {
            Say "  Bỏ qua $arch (Windows này không chạy được)."
            continue
        }
        $code = Invoke-Regsvr32 -Exe $exe -Dll $dll
        if ($code -ne 0) { throw "Đăng ký $arch thất bại (regsvr32 trả $code)." }
        Say "  Đã đăng ký bản $arch."
        $done++
    }
    if ($done -eq 0) { throw 'Không đăng ký được kiến trúc nào.' }
}

function Unregister-Dll {
    param([string]$Root)
    foreach ($arch in @('x64', 'x86')) {
        $dll = Join-Path $Root "$arch\HodionKey.dll"
        if (-not (Test-Path -LiteralPath $dll)) { continue }
        $exe = Get-Regsvr32 -Arch $arch
        if (-not $exe) { continue }
        Invoke-Regsvr32 -Exe $exe -Dll $dll -Unregister | Out-Null
    }
}

function Write-UninstallEntry {
    param([string[]]$Arches)
    $mainArch = if ($Arches -contains 'x64') { 'x64' } else { 'x86' }
    $exe = Join-Path $InstallDir "$mainArch\HodionKeyConfig.exe"
    $version = Get-FileVersionString -Path $exe
    $uninstall = Join-Path $InstallDir 'GoCaiDat.ps1'
    $sizeKb = 0
    try {
        $bytes = (Get-ChildItem -LiteralPath $InstallDir -Recurse -File |
                  Measure-Object -Property Length -Sum).Sum
        $sizeKb = [int]($bytes / 1KB)
    }
    catch { }

    New-Item -Path $UninstallKey -Force | Out-Null
    $values = @{
        DisplayName          = 'HodionKey — bộ gõ tiếng Việt'
        DisplayVersion       = $version
        Publisher            = 'HodionKey'
        InstallLocation      = $InstallDir
        DisplayIcon          = $exe
        UninstallString      = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$uninstall`""
        QuietUninstallString = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$uninstall`" -Quiet"
        URLInfoAbout         = 'https://github.com/phankhanhhung/hodion_key'
    }
    foreach ($name in $values.Keys) {
        New-ItemProperty -Path $UninstallKey -Name $name -Value $values[$name] `
            -PropertyType String -Force | Out-Null
    }
    foreach ($name in @('NoModify', 'NoRepair')) {
        New-ItemProperty -Path $UninstallKey -Name $name -Value 1 `
            -PropertyType DWord -Force | Out-Null
    }
    if ($sizeKb -gt 0) {
        New-ItemProperty -Path $UninstallKey -Name 'EstimatedSize' -Value $sizeKb `
            -PropertyType DWord -Force | Out-Null
    }
    Say "  Đã ghi mục gỡ cài đặt (phiên bản $version)."
}

# --------------------------------------------------------- pha NGƯỜI DÙNG --

function Add-VietnameseLanguage {
    if (-not (Get-Command -Name 'Get-WinUserLanguageList' -ErrorAction SilentlyContinue)) {
        Warn 'Không gọi được Get-WinUserLanguageList — hãy tự thêm "Tiếng Việt" trong Settings > Time & language.'
        return
    }
    try {
        $list = Get-WinUserLanguageList
        if ($list | Where-Object { $_.LanguageTag -like 'vi*' }) {
            Say '  Tiếng Việt đã có trong danh sách ngôn ngữ.'
            return
        }
        $list.Add('vi-VN')
        Set-WinUserLanguageList -LanguageList $list -Force
        Say '  Đã thêm tiếng Việt vào danh sách ngôn ngữ.'
    }
    catch {
        Warn "Không thêm được tiếng Việt tự động ($($_.Exception.Message)). Thêm tay trong Settings > Time & language."
    }
}

function Get-TrayExe {
    $order = if ([Environment]::Is64BitOperatingSystem) { @('x64', 'x86') } else { @('x86') }
    foreach ($arch in $order) {
        $exe = Join-Path $InstallDir "$arch\HodionKeyConfig.exe"
        if (Test-Path -LiteralPath $exe) { return $exe }
    }
    return $null
}

function Start-Tray {
    param([string]$Exe)
    if (Test-Admin) {
        # KHÔNG chạy thẳng: tiến trình con sẽ thừa hưởng quyền admin, và khi
        # đó named pipe của nó nằm ở mức toàn vẹn cao hơn ứng dụng thường —
        # các ứng dụng đang gõ sẽ không nối vào được. explorer.exe chạy dưới
        # quyền người dùng nên tiến trình nó đẻ ra cũng vậy.
        Start-Process -FilePath (Join-Path $env:SystemRoot 'explorer.exe') `
            -ArgumentList "`"$Exe`""
        Say '  Đã bật tiến trình nền (qua explorer, để nó chạy quyền thường).'
    }
    else {
        Start-Process -FilePath $Exe | Out-Null
        Say '  Đã bật tiến trình nền.'
    }
}

function Install-User {
    $exe = Get-TrayExe
    if (-not $exe) {
        throw "Không thấy HodionKeyConfig.exe trong $InstallDir — pha máy chưa chạy xong?"
    }

    if ($NoLanguage) { Say '  Bỏ qua bước thêm ngôn ngữ.' } else { Add-VietnameseLanguage }

    if ($NoAutoStart) {
        Say '  Bỏ qua bước khởi động cùng Windows.'
    }
    else {
        New-Item -Path $RunKey -Force | Out-Null
        New-ItemProperty -Path $RunKey -Name $AppName -Value "`"$exe`"" `
            -PropertyType String -Force | Out-Null
        Say '  Đã đặt khởi động cùng Windows (tắt được trong menu khay).'
    }

    if ($NoLaunch) { Say '  Bỏ qua bước bật tiến trình nền.' } else { Stop-Tray; Start-Tray -Exe $exe }
}

# ------------------------------------------------------------------- chạy --

function Invoke-ElevatedMachinePhase {
    $log = Join-Path $env:TEMP 'HodionKey-CaiDat.log'
    Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue
    $self = Join-Path $PSScriptRoot 'CaiDat.ps1'
    $argList = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', "`"$self`"",
        '-Phase', 'Machine',
        '-InstallDir', "`"$InstallDir`"",
        '-LogFile', "`"$log`""
    )
    if ($Quiet) { $argList += '-Quiet' }

    Say 'Xin quyền quản trị cho phần chép file và đăng ký…'
    try {
        $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList `
            -Verb RunAs -Wait -PassThru
    }
    catch {
        throw 'Bạn đã từ chối cửa sổ xin quyền — không cài được.'
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
    Say ''
    Say "=== $AppName — cài đặt ==="
    Say ''
    switch ($Phase) {
        'Machine' { Install-Machine }
        'User' { Install-User }
        'All' {
            if (Test-Admin) { Install-Machine } else { Invoke-ElevatedMachinePhase }
            Install-User
        }
    }

    if ($Phase -ne 'Machine') {
        Say ''
        Say 'Xong. Cách dùng:'
        Say '  * Win + Space  chuyển sang bàn phím "HodionKey"'
        Say '  * Ctrl + Space bật/tắt gõ tiếng Việt'
        Say '  * Icon V/E ở khay hệ thống: bấm trái đổi chế độ, phải mở menu'
        Say ''
        Say "  Đã cài vào: $InstallDir"
        if ($script:NeedReboot) {
            Say ''
            Say '  LƯU Ý: có file đang được dùng nên bản mới chỉ có hiệu lực'
            Say '  sau khi bạn khởi động lại máy.'
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
