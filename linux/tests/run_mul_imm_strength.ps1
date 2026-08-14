param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_mul_imm_strength.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_mul_imm_strength.c'
$out = Join-Path $PSScriptRoot 'regression-out\mul-imm-strength'
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
    $obj = Join-Path $root 'out\regression_mul_imm_strength.obj'
    $exe = Join-Path $out "mul_imm_strength_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {
        throw "O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}

$asm = Join-Path $out 'O2.asm'
& $Mira -S $source $asm -O2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'O2 assembly build failed' }
$imul = @((Get-Content $asm) | Select-String '(?i)^\s*imul\s').Count
$shl = @((Get-Content $asm) | Select-String '(?i)^\s*(shl|sal)\s').Count
$neg = @((Get-Content $asm) | Select-String '(?i)^\s*neg\s').Count
if ($imul -ne 1 -or $shl -ne 5 -or $neg -ne 4) {
    throw "shape mismatch imul=$imul shl=$shl neg=$neg"
}
Write-Output "SHAPE PASS imul=1 shl=5 neg=4"
