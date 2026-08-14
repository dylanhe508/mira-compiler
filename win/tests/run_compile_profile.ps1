$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$mira = Join-Path $root 'mira.exe'
$case = Join-Path $PSScriptRoot 'regression_phi_inline.mira'
$work = Join-Path $PSScriptRoot 'regression-out\compile-profile'

foreach ($path in @($mira, $case)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing: $path" }
}
New-Item -ItemType Directory -Force -Path $work | Out-Null

$oldProfile = $env:MIRA_COMPILE_PROFILE
try {
    $env:MIRA_COMPILE_PROFILE = '1'
    Push-Location $work
    try {
        $start = [Diagnostics.ProcessStartInfo]::new()
        $start.FileName = $mira
        $start.Arguments = "-O3 `"$case`""
        $start.UseShellExecute = $false
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($start)
        $null = $process.StandardOutput.ReadToEnd()
        $text = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "profile compile failed: $($process.ExitCode)`n$text"
        }
    } finally {
        Pop-Location
    }
} finally {
    $env:MIRA_COMPILE_PROFILE = $oldProfile
}

if ($text -notmatch 'codegen-profile ') {
    throw 'missing codegen-profile line'
}
foreach ($name in @('build','closure','ref','opt','phi-map','regalloc','lower','machine','total')) {
    $escaped = [regex]::Escape($name)
    if ($text -notmatch "(?:^| )${escaped}=[0-9]+(?:\.[0-9]+)?") {
        throw "missing codegen profile field: $name"
    }
}

Write-Output 'CODEGEN PROFILE PASS'
