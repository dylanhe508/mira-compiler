$ErrorActionPreference = 'Continue'

$root = Split-Path -Parent $PSScriptRoot
$mira = Join-Path $root 'mira.exe'
$fixture = Join-Path $PSScriptRoot 'regression_phi_inline.mira'
$outDir = Join-Path $PSScriptRoot 'regression-out\removed-cli'
$irPath = Join-Path $outDir 'explicit-internal.ir'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

function Assert-RemovedOption([string]$option) {
    $output = & $mira $option $fixture 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0) {
        throw "$option unexpectedly succeeded"
    }
    if ($output -notmatch "unsupported option") {
        throw "$option did not report an unsupported option: $output"
    }
}

Assert-RemovedOption '-i'
Assert-RemovedOption '--dump-asm'

Remove-Item -LiteralPath $irPath -ErrorAction SilentlyContinue
$output = & $mira --emit=ir $fixture -o $irPath 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "--emit=ir failed: $output"
}
if (!(Test-Path -LiteralPath $irPath) -or (Get-Item -LiteralPath $irPath).Length -eq 0) {
    throw '--emit=ir did not create a nonempty output file'
}
if ((Get-Content -Raw -LiteralPath $irPath) -notmatch '^;; Mira IR dump') {
    throw '--emit=ir output does not start with the Mira IR header'
}

Write-Output 'REMOVED CLI FEATURES PASS'
