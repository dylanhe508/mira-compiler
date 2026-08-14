param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }

$source = Join-Path $PSScriptRoot 'regression_phi_inline.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_phi_inline.c'
$oracleExe = Join-Path $PSScriptRoot 'regression_phi_inline_oracle.exe'
$miraExe = Join-Path $root 'regression_phi_inline.exe'
$expected = '206'

foreach ($path in @($Mira, $source, $oracleSource)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing required file: $path" }
}

& $Gcc -std=c11 -O2 $oracleSource -o $oracleExe
if ($LASTEXITCODE -ne 0) { throw 'C oracle build failed' }

$oracle = ((& $oracleExe | Out-String) -split '\s+' | Where-Object { $_ }) -join ','
if ($LASTEXITCODE -ne 0) { throw 'C oracle execution failed' }
if ($oracle -ne $expected) { throw "C oracle expected=$expected actual=$oracle" }

Push-Location $root
try {
    foreach ($level in 0..3) {
        & $Mira $source "-O$level"
        if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }
        if (-not (Test-Path -LiteralPath $miraExe)) {
            throw "Mira O$level did not produce $miraExe"
        }

        $actual = ((& $miraExe | Out-String) -split '\s+' | Where-Object { $_ }) -join ','
        if ($LASTEXITCODE -ne 0) { throw "Mira O$level execution failed" }
        if ($actual -ne $oracle) { throw "Mira O$level expected=$oracle actual=$actual" }

        Write-Output "O$level PASS $actual"
    }
}
finally {
    Pop-Location
}
