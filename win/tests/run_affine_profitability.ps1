param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_affine_profitability.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_affine_profitability.c'
$out = Join-Path $PSScriptRoot 'regression-out\affine-profitability'
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
    $obj = Join-Path $root 'out\regression_affine_profitability.obj'
    $exe = Join-Path $out "affine_profitability_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {
        throw "O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}

function Count-Multiply([string]$Asm) {
    @((Get-Content -LiteralPath $Asm) | Select-String '(?i)^\s*imul\s').Count
}
$enabledAsm = Join-Path $out 'enabled.asm'
$disabledAsm = Join-Path $out 'disabled.asm'
& $Mira -S $source $enabledAsm -O3 | Out-Host
$env:MIRA_DECISION_DISABLE = 'affine'
try { & $Mira -S $source $disabledAsm -O3 | Out-Host }
finally { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }
$enabled = Count-Multiply $enabledAsm
$disabled = Count-Multiply $disabledAsm
if ($enabled -ne 2 -or $disabled -ne 8) {
    throw "shape mismatch enabled_imul=$enabled disabled_imul=$disabled"
}
Write-Output "SHAPE PASS enabled_imul=2 disabled_imul=8"
