param([int]$Seconds = 30)
$start = Get-Date
$mounted = $false
for ($i = 0; $i -lt $Seconds; $i++) {
    Start-Sleep -Milliseconds 800
    $el = [int]((Get-Date) - $start).TotalSeconds
    # 卷挂载完成 = Get-Volume 返回 FileSystem(如 exFAT)
    $v = Get-Volume -DriveLetter E -ErrorAction SilentlyContinue
    if ($v -and $v.FileSystem) {
        Write-Output ("t+{0}s: MOUNTED FS={1} {2}GB" -f $el, $v.FileSystem, [math]::Round($v.Size/1GB,0))
        $mounted = $true
        break
    }
    # 盘符出现(可能早于卷就绪,仅参考)
    $p = Test-Path 'E:\' -ErrorAction SilentlyContinue
    if ($i % 3 -eq 2) {
        $pstr = if ($p) { "path=Y" } else { "path=N" }
        Write-Output ("t+{0}s: waiting {1}" -f $el, $pstr)
    }
}
if (-not $mounted) { Write-Output "NOT MOUNTED in $Seconds s" }
