param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_cross_branch_affine.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_cross_branch_affine.c'
$out = Join-Path $PSScriptRoot 'regression-out\cross-branch-affine'
$runtime = @('rt_core.obj','rt_print.obj','rt_win.obj','rt_mem.obj') |
    ForEach-Object { Join-Path $root "runtime\$_" }
foreach ($path in @($Mira, $source, $oracleSource) + $runtime) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing: $path" }
}
[void][IO.Directory]::CreateDirectory($out)
$oracle = Join-Path $out 'oracle.exe'
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()

foreach ($level in 0..3) {
    Push-Location $root
    try { & $Mira "-O$level" $source | Out-Host }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }
    $obj = Join-Path $root 'out\regression_cross_branch_affine.obj'
    $exe = Join-Path $out "cross_branch_affine_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {
        throw "O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}

$enabledAsm = Join-Path $out 'enabled.asm'
$disabledAsm = Join-Path $out 'disabled.asm'
& $Mira -S $source $enabledAsm -O3 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'enabled assembly build failed' }
$env:MIRA_DECISION_DISABLE = 'affine'
try { & $Mira -S $source $disabledAsm -O3 | Out-Host }
finally { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }
if ($LASTEXITCODE -ne 0) { throw 'disabled assembly build failed' }
$enabledLines = Get-Content -LiteralPath $enabledAsm
$disabledLines = Get-Content -LiteralPath $disabledAsm
$telemetryStart = ($enabledLines | Select-String '^branchy_recurrence:').LineNumber
$repeatStart = ($enabledLines | Select-String '^branchy_repeated:').LineNumber
$exclusiveStart = ($enabledLines | Select-String '^branchy_exclusive:').LineNumber
$mainStart = ($enabledLines | Select-String '^mira_main:').LineNumber
$disabledTelemetryStart = ($disabledLines | Select-String '^branchy_recurrence:').LineNumber
$disabledRepeatStart = ($disabledLines | Select-String '^branchy_repeated:').LineNumber
$disabledExclusiveStart = ($disabledLines | Select-String '^branchy_exclusive:').LineNumber
$disabledMainStart = ($disabledLines | Select-String '^mira_main:').LineNumber
$body = $enabledLines[($telemetryStart - 1)..($repeatStart - 2)]
$disabledBody = $disabledLines[($disabledTelemetryStart - 1)..($disabledRepeatStart - 2)]
$repeatBody = $enabledLines[($repeatStart - 1)..($exclusiveStart - 2)]
$disabledRepeatBody = $disabledLines[($disabledRepeatStart - 1)..($disabledExclusiveStart - 2)]
$exclusiveBody = $enabledLines[($exclusiveStart - 1)..($mainStart - 2)]
$disabledExclusiveBody = $disabledLines[($disabledExclusiveStart - 1)..($disabledMainStart - 2)]
$singleEnabled = @($body | Select-String '(?i)^\s*imul\s').Count
$singleDisabled = @($disabledBody | Select-String '(?i)^\s*imul\s').Count
$repeatEnabled = @($repeatBody | Select-String '(?i)^\s*imul\s').Count
$repeatDisabled = @($disabledRepeatBody | Select-String '(?i)^\s*imul\s').Count
$exclusiveEnabled = @($exclusiveBody | Select-String '(?i)^\s*imul\s').Count
$exclusiveDisabled = @($disabledExclusiveBody | Select-String '(?i)^\s*imul\s').Count
if ($singleEnabled -ne $singleDisabled) {
    throw "single-use branch recurrence should be rejected enabled=$singleEnabled disabled=$singleDisabled"
}
if ($repeatEnabled -ge $repeatDisabled) {
    throw "repeated branch recurrence missing enabled=$repeatEnabled disabled=$repeatDisabled"
}
if ($exclusiveEnabled -ne $exclusiveDisabled) {
    throw "mutually exclusive recurrence should be rejected enabled=$exclusiveEnabled disabled=$exclusiveDisabled"
}
$cmov = @($repeatBody | Select-String '(?i)^\s*cmov').Count
if ($cmov -ne 0) {
    throw "periodic branch was if-converted cmov=$cmov"
}
$spills = @($body | Select-String '\[rbp \+ -').Count
if ($spills -ne 0) {
    throw "branch selector spilled after recurrence spills=$spills"
}
Write-Output "SHAPE PASS single=$singleEnabled/$singleDisabled repeated=$repeatEnabled/$repeatDisabled exclusive=$exclusiveEnabled/$exclusiveDisabled cmov=0 spills=0"
