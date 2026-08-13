param([string]$Compiler = (Join-Path (Split-Path -Parent $PSScriptRoot) 'mira.exe'))
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'vector_add_local.mira'
$root = Split-Path -Parent $PSScriptRoot

function Median([long[]]$values) {
    $sorted = @($values | Sort-Object)
    $sorted[[int]($sorted.Count / 2)]
}

Push-Location $root
try {
    & $Compiler $source -O3 -mavx2 | Out-Null
    Move-Item '.\vector_add_local.exe' (Join-Path $PSScriptRoot 'vector_perf_avx.exe') -Force
    & $Compiler $source -O3 -mno-avx2 | Out-Null
    Move-Item '.\vector_add_local.exe' (Join-Path $PSScriptRoot 'vector_perf_scalar.exe') -Force
} finally { Pop-Location }

$avx = @(); $scalar = @(); $expected = '4398050705408'
1..9 | ForEach-Object {
    $a = @(& (Join-Path $PSScriptRoot 'vector_perf_avx.exe'))
    $s = @(& (Join-Path $PSScriptRoot 'vector_perf_scalar.exe'))
    if ($a[0] -ne $expected -or $s[0] -ne $expected) { throw 'vector checksum mismatch' }
    if ($_ -gt 2) { $avx += [long]$a[1]; $scalar += [long]$s[1] }
}
$avxMedian = Median $avx
$scalarMedian = Median $scalar
Write-Host "avx_ns=$avxMedian scalar_ns=$scalarMedian"
if ($avxMedian * 100 -ge $scalarMedian * 90) {
    throw 'AVX2 path failed to beat scalar by at least 10 percent'
}
