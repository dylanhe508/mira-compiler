param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_infix_line_continuation.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_infix_line_continuation.c'
$out = Join-Path $PSScriptRoot 'regression-out\infix-line-continuation'
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
    $obj = Join-Path $root 'out\regression_infix_line_continuation.obj'
    $exe = Join-Path $out "infix_line_continuation_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {
        throw "O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}
