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

Compile-And-Run 'adjacent_calls_valid' '0' '-O0'

Expect-Compile-Error 'unknown_alias_error' "unknown module alias 'missing'"
Expect-Compile-Error 'unknown_member_error' "unknown module member 'left.missing'"
Expect-Compile-Error 'type_error_import' "function 'type_error_dep.bad': expected str, got i64"
Expect-Compile-Error 'arity_error_import' "function 'arity_error_dep.pair' expects 2 arguments, got 3"
Expect-Compile-Error 'missing_result_import' "function 'missing_result_dep.missing': expected i64, got void"
Expect-Compile-Error 'ambiguous_missing_import' "function 'ambiguous_missing_a.missing': expected i64, got void"
Expect-Compile-Error 'adjacent_calls_error' "function 'adjacent_two.same' expects 2 arguments, got 1"

Push-Location $moduleRoot
try {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $typeError = (& $compiler 'type_error_import.mira' 2>&1) -join "`n"
    $typeErrorExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($typeErrorExitCode -eq 0) { throw 'imported type error unexpectedly compiled' }
    if ($typeError -notmatch 'type_error_dep\.mira' -or $typeError -notmatch 'Line 3,') {
        throw "imported type error has wrong provenance: $typeError"
    }
    if ($typeError -notmatch [regex]::Escape('fn bad() -> str { 42 }')) {
        throw "imported type error has wrong source line: $typeError"
    }

    $ErrorActionPreference = 'Continue'
    $arityError = (& $compiler 'arity_error_import.mira' 2>&1) -join "`n"
    $arityErrorExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($arityErrorExitCode -eq 0) { throw 'imported arity error unexpectedly compiled' }
    if ($arityError -notmatch 'arity_error_dep\.mira' -or
        $arityError -notmatch [regex]::Escape('Line 2, Column 19')) {
        throw "imported arity error has wrong provenance: $arityError"
    }

    $ErrorActionPreference = 'Continue'
    $missingError = (& $compiler 'missing_result_import.mira' 2>&1) -join "`n"
    $missingErrorExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($missingErrorExitCode -eq 0) { throw 'imported missing-result error unexpectedly compiled' }
    if ($missingError -notmatch 'missing_result_dep\.mira' -or
        $missingError -notmatch [regex]::Escape('fn missing() -> i64 {}')) {
        throw "imported missing-result error has wrong provenance: $missingError"
    }

    $ErrorActionPreference = 'Continue'
    $ambiguousError = (& $compiler 'ambiguous_missing_import.mira' 2>&1) -join "`n"
    $ambiguousErrorExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($ambiguousErrorExitCode -eq 0) { throw 'ambiguous missing-result error unexpectedly compiled' }
    if ($ambiguousError -notmatch 'ambiguous_missing_a\.mira' -or
        $ambiguousError -match 'ambiguous_missing_b\.mira') {
        throw "ambiguous missing-result error has wrong provenance: $ambiguousError"
    }

    $ErrorActionPreference = 'Continue'
    $adjacentError = (& $compiler 'adjacent_calls_error.mira' 2>&1) -join "`n"
    $adjacentErrorExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($adjacentErrorExitCode -eq 0) { throw 'adjacent-call arity error unexpectedly compiled' }
    if ($adjacentError -notmatch [regex]::Escape('Line 3, Column 32')) {
        throw "adjacent-call arity error has wrong call location: $adjacentError"
    }
} finally {
    $ErrorActionPreference = 'Stop'
    Pop-Location
}
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
