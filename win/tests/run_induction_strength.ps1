param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }

$source = Join-Path $PSScriptRoot 'regression_induction_strength.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_induction_strength.c'
$out = Join-Path $PSScriptRoot 'regression-out\induction-strength'
$oracle = Join-Path $out 'regression_induction_strength_oracle.exe'

foreach ($path in @($Mira, $source, $oracleSource)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing required file: $path" }
}

$runtime = @(
    (Join-Path $root 'runtime\rt_core.obj'),
    (Join-Path $root 'runtime\rt_print.obj'),
    (Join-Path $root 'runtime\rt_win.obj'),
    (Join-Path $root 'runtime\rt_mem.obj')
)
foreach ($path in $runtime) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing Mira runtime object: $path" }
}

[void][IO.Directory]::CreateDirectory($out)
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'GCC oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()
if ($LASTEXITCODE -ne 0) { throw 'GCC oracle run failed' }

foreach ($level in 0..3) {
    Push-Location $root
    try {
        & $Mira "-O$level" $source | Out-Host
    }
    finally {
        Pop-Location
    }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }

    $obj = Join-Path $root 'out\regression_induction_strength.obj'
    if (-not (Test-Path -LiteralPath $obj)) { throw "Mira O$level did not produce $obj" }
    $exe = Join-Path $out "regression_induction_strength_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }

    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level run failed" }
    if ($actual -ne $expected) {
        throw "Mira O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}
