param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc',
    [ValidateSet('declarations', 'calls')]
    [string]$Group = 'declarations'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$types = Join-Path $PSScriptRoot 'types'

foreach ($path in @($Mira, $types)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing required path: $path" }
}

function Expect-Compile-Error([string]$name, [string]$fragment, [string]$location = '') {
    Push-Location $types
    try {
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $message = (& $Mira "$name.mira" 2>&1) -join "`n"
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        if ($exitCode -eq 0) { throw "$name unexpectedly compiled" }
        if ($message -notmatch [regex]::Escape($fragment)) {
            throw "$name missing diagnostic '$fragment': $message"
        }
        if ($location -and $message -notmatch [regex]::Escape($location)) {
            throw "$name missing location '$location': $message"
        }
    } finally {
        $ErrorActionPreference = 'Stop'
        Pop-Location
    }
}

if ($Group -eq 'declarations') {
    Expect-Compile-Error 'unknown_parameter_type' "unknown type 'number'"
    Expect-Compile-Error 'unknown_local_type' "unknown type 'number'"
    Expect-Compile-Error 'unknown_struct_field_type' "unknown type 'number'"
    Expect-Compile-Error 'unknown_struct_field_unknown' "unknown type 'unknown'"
    Expect-Compile-Error 'void_struct_field_type' "type 'void' is only valid as a function result"

    Push-Location $types
    try {
        & $Mira -O0 'annotations_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'annotations_valid O0 compile failed' }
        $actual = ((& (Join-Path $types 'annotations_valid.exe')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'annotations_valid O0 run failed' }
        if ($actual -ne '7') { throw "annotations_valid output '$actual', expected '7'" }
    } finally {
        Pop-Location
    }

    $metadataExe = Join-Path $types 'type_metadata_test.exe'
    & $Gcc -O0 "-I$root" (Join-Path $PSScriptRoot 'type_metadata_test.c') `
        (Join-Path $root 'lexer.c') (Join-Path $root 'parser.c') `
        (Join-Path $root 'typecheck.c') (Join-Path $root 'memory.c') `
        (Join-Path $root 'error.c') (Join-Path $root 'hash.c') -o $metadataExe
    if ($LASTEXITCODE -ne 0) { throw 'type metadata test build failed' }
    & $metadataExe | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'type metadata test failed' }

    $freeMetadataExe = Join-Path $types 'program_free_type_metadata_test.exe'
    & $Gcc -O0 "-I$root" (Join-Path $PSScriptRoot 'program_free_type_metadata_test.c') `
        -o $freeMetadataExe
    if ($LASTEXITCODE -ne 0) { throw 'program free metadata test build failed' }
    & $freeMetadataExe
    if ($LASTEXITCODE -ne 0) { throw "program free metadata test failed: $LASTEXITCODE" }
    Write-Output 'PROGRAM FREE TYPE METADATA PASS'

    Write-Output 'GRADUAL TYPE DECLARATIONS PASS'
}

if ($Group -eq 'calls') {
    Expect-Compile-Error 'call_extra_argument_error' "expects 1 arguments, got 2" 'Line 2, Column 19'
    Expect-Compile-Error 'call_zero_arity_error' "expects 0 arguments, got 1" 'Line 2, Column 19'
    Expect-Compile-Error 'call_nested_arity_error' "expects 2 arguments, got 1" 'Line 3, Column 28'
    Expect-Compile-Error 'call_string_paren_arity_error' "expects 1 arguments, got 2" 'Line 2, Column 19'
    Expect-Compile-Error 'call_comment_arity_error' "expects 2 arguments, got 3" 'Line 3, Column 19'
    Expect-Compile-Error 'call_list_arity_error' "expects 2 arguments, got 3"
    Expect-Compile-Error 'list_void_value_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'condition_void_value_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'void_later_argument_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'branch_void_result_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'branch_tail_type_error' "function 'choose': expected i64, got str"
    Expect-Compile-Error 'branch_single_arm_result_error' "function 'maybe': expected i64, got void"
    Expect-Compile-Error 'branch_partial_return_error' "function 'partial': expected i64, got void"
    Expect-Compile-Error 'missing_result_error' "function 'missing': expected i64, got void"
    Expect-Compile-Error 'void_value_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'return_type_error' "function 'label': expected str, got i64"
    Expect-Compile-Error 'call_arity_error' "expects 2 arguments, got 1"
    Expect-Compile-Error 'call_argument_type_error' "argument 1 of 'square': expected f64, got i64"
    Expect-Compile-Error 'extern_signature_error' "argument 1 of 'mira_abs': expected f64, got i64"

    Push-Location $types
    try {
        & $Mira -O0 'extern_signature_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'extern_signature_valid O0 compile failed' }
        $actual = ((& (Join-Path $types 'extern_signature_valid.exe')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'extern_signature_valid O0 run failed' }
        if ($actual -ne '42') { throw "extern_signature_valid output '$actual', expected '42'" }
    } finally {
        Pop-Location
    }

    Push-Location $types
    try {
        & $Mira -O0 'legacy_nested_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'legacy_nested_valid O0 compile failed' }

        & $Mira -O0 'branch_all_paths_return_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'branch_all_paths_return_valid O0 compile failed' }
        $actual = ((& (Join-Path $types 'branch_all_paths_return_valid.exe')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'branch_all_paths_return_valid O0 run failed' }
        if ($actual -ne '1') { throw "branch_all_paths_return_valid output '$actual', expected '1'" }
    } finally {
        Pop-Location
    }

    Write-Output 'GRADUAL TYPE CALLS PASS'
}
