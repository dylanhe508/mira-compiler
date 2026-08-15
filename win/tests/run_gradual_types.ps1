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

function Expect-Compile-Error([string]$name, [string]$fragment) {
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
    Expect-Compile-Error 'void_value_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'return_type_error' "function 'label': expected str, got i64"
    Expect-Compile-Error 'call_arity_error' "expects 2 arguments, got 1"
    Expect-Compile-Error 'call_argument_type_error' "argument 1 of 'square': expected f64, got i64"

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

    Write-Output 'GRADUAL TYPE CALLS PASS'
}
