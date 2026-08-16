param([string]$Mira = '')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$isWindowsVariable = Get-Variable -Name IsWindows -ErrorAction SilentlyContinue
$isWindowsHost = if ($null -ne $isWindowsVariable) {
    [bool]$isWindowsVariable.Value
} else {
    $env:OS -eq 'Windows_NT'
}
$suffix = if ($isWindowsHost) { '.exe' } else { '' }
if (-not $Mira) { $Mira = Join-Path $root "mira$suffix" }
$types = Join-Path $PSScriptRoot 'types'

Push-Location $types
try {
    $irPath = Join-Path $types 'shift_immediate_codegen.ir'
    & $Mira -O3 'shift_immediate_codegen.mira' | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'shift immediate executable compile failed' }
    & $Mira -S 'shift_immediate_codegen.mira' $irPath -O3 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'shift immediate compile failed' }
    $program = Join-Path $types "shift_immediate_codegen$suffix"
    $actual = ((& $program) -join "`n").Trim()
    if ($LASTEXITCODE -ne 0) { throw 'shift immediate run failed' }
    if ($actual -ne "8`n8192") { throw "shift output mismatch: $actual" }

    $ir = Get-Content -Raw -LiteralPath $irPath
    $match = [regex]::Match($ir,
        '(?ms)^shift_constants:\r?\n(?<body>.*?)(?=^[A-Za-z_][A-Za-z0-9_]*:\r?$|\z)')
    if (-not $match.Success) { throw 'shift_constants assembly body missing' }
    $body = $match.Groups['body'].Value
    $rcxSaves = ([regex]::Matches($body, '(?m)^\s*push rcx\r?$')).Count
    if ($rcxSaves -ne 0) {
        throw "constant shifts still use RCX save path: expected 0, got $rcxSaves"
    }
    if ($body -notmatch '(?m)^\s*shl r[a-z0-9]+, 3\r?$') {
        throw 'constant shifts are not encoded with immediate counts'
    }
    if ($ir -notmatch '(?m)^\s*push rcx\r?$') {
        throw 'dynamic shift no longer uses the required CL path'
    }
    Write-Output 'SHIFT IMMEDIATE CODEGEN PASS'
} finally {
    Pop-Location
}
