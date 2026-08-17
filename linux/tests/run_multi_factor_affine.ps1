param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_multi_factor_affine.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_multi_factor_affine.c'
$out = Join-Path $PSScriptRoot 'regression-out\multi-factor-affine'
$runtime = @(
    (Join-Path $root 'runtime\rt_core.obj'),
    (Join-Path $root 'runtime\rt_print.obj'),
    (Join-Path $root 'runtime\rt_win.obj'),
    (Join-Path $root 'runtime\rt_mem.obj')
)
foreach ($path in @($Mira, $source, $oracleSource) + $runtime) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing required file: $path" }
}
[void][IO.Directory]::CreateDirectory($out)

$oracle = Join-Path $out 'oracle.exe'
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()
if ($LASTEXITCODE -ne 0) { throw 'oracle run failed' }

foreach ($level in 0..3) {
    Push-Location $root
    try { & $Mira "-O$level" $source | Out-Host }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }
    $obj = Join-Path $root 'out\regression_multi_factor_affine.obj'
    $exe = Join-Path $out "multi_factor_affine_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level run failed" }
    if ($actual -ne $expected) {
        throw "Mira O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}

function Count-Multiply([string]$Asm) {
    return @((Get-Content -LiteralPath $Asm) | Select-String '(?i)^\s*imul\s').Count
}

$enabledAsm = Join-Path $out 'enabled.asm'
$disabledAsm = Join-Path $out 'disabled.asm'
& $Mira -S $source -o $enabledAsm -O3 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'enabled assembly build failed' }
$env:MIRA_DECISION_DISABLE = 'affine'
try { & $Mira -S $source -o $disabledAsm -O3 | Out-Host }
finally { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }
if ($LASTEXITCODE -ne 0) { throw 'disabled assembly build failed' }
$enabled = Count-Multiply $enabledAsm
$disabled = Count-Multiply $disabledAsm
if ($enabled -ne 0 -or $disabled -ne 4) {
    throw "shape mismatch enabled_imul=$enabled disabled_imul=$disabled"
}
Write-Output "SHAPE PASS enabled_imul=0 disabled_imul=4"
