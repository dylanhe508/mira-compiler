param(
    [ValidateSet('pipeline', 'ir-dump', 'all')]
    [string]$Group = 'all',
    [string]$Mira = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$fixture = Join-Path $PSScriptRoot 'final_ir_pipeline.mira'
$outDir = Join-Path $PSScriptRoot 'regression-out\cli-emit-modes'
$irPath = Join-Path $outDir 'final_ir_pipeline.ir'
$irDumpTest = Join-Path $outDir 'ir_dump_test.exe'
$irDumpSink = Join-Path $outDir 'invalid.ir'

try {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    if ($Group -in @('pipeline', 'all')) {
        & $Mira --emit=ir $fixture -o $irPath -O3 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw '--emit=ir failed' }
        if (!(Test-Path -LiteralPath $irPath)) { throw '--emit=ir did not create output' }
        $ir = Get-Content -Raw -LiteralPath $irPath
        if ($ir -notmatch '^;; Mira IR dump') { throw '--emit=ir did not write Mira IR' }
        if ($ir -match '(?m)^\s*add\s+[^,]+,\s*0\s*$') {
            throw 'late add-zero cleanup did not reach emitted IR'
        }
        Write-Output 'FINAL IR PIPELINE PASS'
    }
    if ($Group -in @('ir-dump', 'all')) {
        & gcc -std=c11 -Wall -Wextra -Werror -I $root `
            (Join-Path $PSScriptRoot 'ir_dump_test.c') `
            (Join-Path $root 'codegen\ir_dump.c') -o $irDumpTest
        if ($LASTEXITCODE -ne 0) { throw 'ir_dump_test build failed' }
        & $irDumpTest $irDumpSink | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'ir_dump_test failed' }
    }
    if ($Group -eq 'all') { Write-Output 'CLI EMIT MODES PASS' }
} finally {
    Remove-Item -LiteralPath $outDir -Recurse -Force -ErrorAction SilentlyContinue
}
