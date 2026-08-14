param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira-next.exe' }
$out = Join-Path $PSScriptRoot 'regression-out'
[void][IO.Directory]::CreateDirectory($out)

function Invoke-Bounded([string]$Exe, [string]$Argument = '') {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Exe
    $info.Arguments = $Argument
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(10000)) { $process.Kill(); throw "timeout: $Exe $Argument" }
    if ($process.ExitCode -ne 0) { throw "exit $($process.ExitCode): $Exe $Argument $($stderr.Result)" }
    $numbers = @($stdout.Result -split "`r?`n" | Where-Object { $_ -match '^-?\d+$' })
    if ($numbers.Count -lt 2) { throw "invalid output: $Exe $Argument $($stdout.Result)" }
    [pscustomobject]@{ Result = [long]$numbers[0]; Ns = [long]$numbers[-1] }
}

function Median([long[]]$Values) { ($Values | Sort-Object)[[int]($Values.Count / 2)] }
function Measure-Case([string]$MiraExe, [string]$GccExe, [string]$Argument) {
    $warmM = Invoke-Bounded $MiraExe
    $warmG = Invoke-Bounded $GccExe $Argument
    if ($warmM.Result -ne $warmG.Result) { throw "$Argument warmup mismatch: Mira=$($warmM.Result) GCC=$($warmG.Result)" }
    $mt = @(); $gt = @()
    1..11 | ForEach-Object {
        $m = Invoke-Bounded $MiraExe
        $g = Invoke-Bounded $GccExe $Argument
        if ($m.Result -ne $g.Result) { throw "$Argument mismatch: Mira=$($m.Result) GCC=$($g.Result)" }
        $mt += $m.Ns; $gt += $g.Ns
    }
    [pscustomobject]@{ Name=$Argument; Result=$warmM.Result; MiraNs=Median $mt; GccNs=Median $gt }
}

$gccExe = Join-Path $out 'gcc_suite.exe'
& $Gcc -O3 -march=native (Join-Path $root 'suite_c.c') -o $gccExe
if ($LASTEXITCODE -ne 0) { throw 'GCC suite build failed' }

$miraCases = @{}
foreach ($case in @('branch_pred','branch_random')) {
    $source = Join-Path $root "suite_$case.mira"
    Push-Location $out
    try { & $Mira -O3 $source | Out-Host }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Mira $case build failed" }
    $miraCases[$case] = Join-Path $out "suite_$case.exe"
}

$results = @(
    Measure-Case $miraCases.branch_pred $gccExe 'branch_pred'
    Measure-Case $miraCases.branch_random $gccExe 'branch_random'
)
'name,result,mira_ns,gcc_ns,ratio'
foreach ($r in $results) {
    $ratio = [math]::Round([double]($r.MiraNs / $r.GccNs), 3)
    "$($r.Name),$($r.Result),$($r.MiraNs),$($r.GccNs),$ratio"
    if ($ratio -gt 5.0) { throw "$($r.Name) performance regression ratio=$ratio" }
}
