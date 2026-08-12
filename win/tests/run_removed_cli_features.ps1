$ErrorActionPreference = 'Continue'

$root = Split-Path -Parent $PSScriptRoot
$mira = Join-Path $root 'mira.exe'
$fixture = Join-Path $PSScriptRoot 'regression_phi_inline.mira'
$outDir = Join-Path $PSScriptRoot 'regression-out\removed-cli'
$asm = Join-Path $outDir 'preserved-S.asm'
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

Remove-Item -LiteralPath $asm -ErrorAction SilentlyContinue
$output = & $mira -S $fixture $asm 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "-S failed: $output"
}
if (!(Test-Path -LiteralPath $asm) -or (Get-Item -LiteralPath $asm).Length -eq 0) {
    throw '-S did not create a nonempty output file'
}

Write-Output 'REMOVED CLI FEATURES PASS'
