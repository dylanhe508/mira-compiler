$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $root 'mira.exe'
$moduleRoot = Join-Path $PSScriptRoot 'modules'

function Compile-And-Run([string]$name, [string]$expected, [string]$opt = '') {
    Push-Location $moduleRoot
    try {
        $args = @()
        if ($opt) { $args += $opt }
        $args += "$name.mira"
        & $compiler @args | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "$name $opt compile failed" }
        $actual = (& "$moduleRoot\$name.exe") -join "`n"
        if ($LASTEXITCODE -ne 0) { throw "$name $opt run failed" }
        if ($actual -ne $expected) { throw "$name $opt output '$actual', expected '$expected'" }
    } finally {
        Pop-Location
    }
}

function Expect-Compile-Error([string]$name, [string]$fragment) {
    Push-Location $moduleRoot
    try {
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $message = (& $compiler "$name.mira" 2>&1) -join "`n"
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        if ($exitCode -eq 0) { throw "$name unexpectedly compiled" }
        if ($message -notmatch [regex]::Escape($fragment)) {
            throw "$name missing diagnostic '$fragment': $message"
        }
    } finally {
        $ErrorActionPreference = 'Stop'
        Pop-Location
    }
}

foreach ($level in 0..3) {
    Compile-And-Run 'import_math' "42`n100" "-O$level"
    Compile-And-Run 'import_alias' '23' "-O$level"
    Compile-And-Run 'import_isolation' '42' "-O$level"
    Compile-And-Run 'import_internal' '42' "-O$level"
    Compile-And-Run 'stdlib_abi_types' "9`n12345`n77" "-O$level"
}
Write-Output 'MODULE NAMESPACE O0-O3 PASS'

Expect-Compile-Error 'unknown_alias_error' "unknown module alias 'missing'"
Expect-Compile-Error 'unknown_member_error' "unknown module member 'left.missing'"
Write-Output 'MODULE DIAGNOSTICS PASS'

Push-Location $moduleRoot
try {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $cycle = (& $compiler 'import_cycle_main.mira' 2>&1) -join "`n"
    $cycleExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($cycleExitCode -eq 0) { throw 'cycle unexpectedly compiled' }
    if ($cycle -notmatch 'module import cycle') { throw "missing cycle diagnostic: $cycle" }
} finally {
    $ErrorActionPreference = 'Stop'
    Pop-Location
}
Write-Output 'MODULE CYCLE PASS'
