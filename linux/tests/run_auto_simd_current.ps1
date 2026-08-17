param([string]$Compiler = (Join-Path (Split-Path -Parent $PSScriptRoot) 'mira.exe'))
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'simd_add.mira'
$avx = Join-Path $PSScriptRoot 'simd_add_avx2.ir'
$scalar = Join-Path $PSScriptRoot 'simd_add_scalar.ir'

& $Compiler --emit=ir $source -o $avx -O3 -mavx2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'AVX2 compilation failed' }
$avxText = Get-Content -Raw -LiteralPath $avx
foreach ($pattern in @('\bymm[0-9]+\b', '\bvmovdqu\b', '\bvpaddq\b', '\bvzeroupper\b')) {
    if ($avxText -notmatch $pattern) { throw "AVX2 output missing $pattern" }
}

& $Compiler --emit=ir $source -o $scalar -O3 -mno-avx2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'scalar compilation failed' }
$scalarText = Get-Content -Raw -LiteralPath $scalar
if ($scalarText -match '\bymm[0-9]+\b|\bvmovdqu\b|\bvpaddq\b') {
    throw 'scalar target emitted AVX2 instructions'
}

Write-Host 'automatic AVX2 and scalar fallback passed'
