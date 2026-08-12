param([int]$Seconds = 25)
$start = Get-Date
$mounted = $false
for ($i = 0; $i -lt $Seconds; $i++) {
    Start-Sleep -Milliseconds 900
    $el = [int]((Get-Date) - $start).TotalSeconds
    $ok = Test-Path 'E:\' -ErrorAction SilentlyContinue
    if ($ok) {
        Write-Output ("t+{0}s: MOUNTED" -f $el)
        $mounted = $true
        break
    }
    if ($i % 3 -eq 2) { Write-Output ("t+{0}s: waiting" -f $el) }
}
if (-not $mounted) { Write-Output "NOT MOUNTED in $Seconds s" }
