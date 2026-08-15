param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc',
    [ValidateSet('all', 'declarations', 'calls', 'expressions', 'ssa')]
    [string]$Group = 'declarations'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$isWindowsVariable = Get-Variable -Name IsWindows -ErrorAction SilentlyContinue
$isWindowsHost = if ($null -ne $isWindowsVariable) {
    [bool]$isWindowsVariable.Value
} else {
    $env:OS -eq 'Windows_NT'
}
$binarySuffix = if ($isWindowsHost) { '.exe' } else { '' }

function Get-BinaryPath([string]$directory, [string]$name) {
    Join-Path $directory "$name$binarySuffix"
}

if (-not $Mira) { $Mira = Get-BinaryPath $root 'mira' }
$types = Join-Path $PSScriptRoot 'types'

if (-not (Test-Path -LiteralPath $Mira)) { throw "missing required path: $Mira" }
$Mira = (Resolve-Path -LiteralPath $Mira).Path
if (-not (Test-Path -LiteralPath $types)) { throw "missing required path: $types" }

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

if ($Group -in @('all', 'declarations')) {
    Expect-Compile-Error 'unknown_parameter_type' "unknown type 'number'"
    Expect-Compile-Error 'unknown_local_type' "unknown type 'number'"
    Expect-Compile-Error 'unknown_struct_field_type' "unknown type 'number'"
    Expect-Compile-Error 'unknown_struct_field_unknown' "unknown type 'unknown'"
    Expect-Compile-Error 'void_struct_field_type' "type 'void' is only valid as a function result"
    Expect-Compile-Error 'typed_const_i64_error' "constant 'bad': expected i64, got f64" 'Line 2, Column 18'
    Expect-Compile-Error 'typed_const_f64_error' "constant 'bad': expected f64, got i64" 'Line 2, Column 22'
    Expect-Compile-Error 'typed_const_str_error' "constant 'bad': expected str, got i64" 'Line 2, Column 18'
    Expect-Compile-Error 'typed_const_bool_error' "constant 'bad': expected bool, got i64" 'Line 2, Column 19'
    Expect-Compile-Error 'typed_const_ptr_error' "constant 'bad': expected ptr, got str" 'Line 2, Column 18'
    Expect-Compile-Error 'typed_const_void_error' "type 'void' is only valid as a function result"

    Push-Location $types
    try {
        & $Mira -O0 'annotations_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'annotations_valid O0 compile failed' }
        $actual = ((& (Get-BinaryPath $types 'annotations_valid')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'annotations_valid O0 run failed' }
        if ($actual -ne '7') { throw "annotations_valid output '$actual', expected '7'" }

        & $Mira -O0 'ptr_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'ptr_valid O0 compile failed' }
        $actual = ((& (Get-BinaryPath $types 'ptr_valid')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'ptr_valid O0 run failed' }
        if ($actual -ne '7') { throw "ptr_valid output '$actual', expected '7'" }
    } finally {
        Pop-Location
    }

    $metadataExe = Get-BinaryPath $types 'type_metadata_test'
    & $Gcc -O0 "-I$root" (Join-Path $PSScriptRoot 'type_metadata_test.c') `
        (Join-Path $root 'lexer.c') (Join-Path $root 'parser.c') `
        (Join-Path $root 'typecheck.c') (Join-Path $root 'memory.c') `
        (Join-Path $root 'error.c') (Join-Path $root 'hash.c') -o $metadataExe
    if ($LASTEXITCODE -ne 0) { throw 'type metadata test build failed' }
    & $metadataExe | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'type metadata test failed' }

    $freeMetadataExe = Get-BinaryPath $types 'program_free_type_metadata_test'
    & $Gcc -O0 "-I$root" (Join-Path $PSScriptRoot 'program_free_type_metadata_test.c') `
        -o $freeMetadataExe
    if ($LASTEXITCODE -ne 0) { throw 'program free metadata test build failed' }
    & $freeMetadataExe
    if ($LASTEXITCODE -ne 0) { throw "program free metadata test failed: $LASTEXITCODE" }
    Write-Output 'PROGRAM FREE TYPE METADATA PASS'

    $lexerStateExe = Get-BinaryPath $types 'lexer_state_test'
    & $Gcc -O0 "-I$root" (Join-Path $PSScriptRoot 'lexer_state_test.c') `
        (Join-Path $root 'lexer.c') (Join-Path $root 'memory.c') `
        (Join-Path $root 'error.c') (Join-Path $root 'hash.c') -o $lexerStateExe
    if ($LASTEXITCODE -ne 0) { throw 'lexer state test build failed' }
    Push-Location $types
    try {
        & $lexerStateExe
        if ($LASTEXITCODE -ne 0) { throw "lexer state test failed: $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
    Write-Output 'LEXER STATE PASS'

    Write-Output 'GRADUAL TYPE DECLARATIONS PASS'
}

if ($Group -in @('all', 'calls')) {
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
    Expect-Compile-Error 'branch_else_type_origin_error' "function 'choose': expected i64, got str" 'Line 5,'
    Expect-Compile-Error 'branch_else_void_origin_error' 'returns void and cannot be used as a value' 'Line 6, Column 9'
    Expect-Compile-Error 'switch_no_result_error' "function 'choose': expected i64, got void"
    Expect-Compile-Error 'switch_missing_path_error' "function 'choose': expected i64, got void"
    Expect-Compile-Error 'switch_tail_type_error' "function 'choose': expected i64, got str"
    Expect-Compile-Error 'switch_void_result_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'try_missing_path_error' "function 'partial': expected i64, got void"
    Expect-Compile-Error 'try_tail_type_error' "function 'attempt': expected i64, got str"
    Expect-Compile-Error 'missing_result_error' "function 'missing': expected i64, got void"
    Expect-Compile-Error 'void_value_error' 'returns void and cannot be used as a value'
    Expect-Compile-Error 'return_type_error' "function 'label': expected str, got i64"
    Expect-Compile-Error 'call_arity_error' "expects 2 arguments, got 1"
    Expect-Compile-Error 'call_argument_type_error' "argument 1 of 'square': expected f64, got i64"
    Expect-Compile-Error 'ptr_argument_type_error' "argument 1 of 'identity': expected ptr, got i64"
    Expect-Compile-Error 'ptr_string_argument_type_error' "argument 1 of 'identity': expected ptr, got str"
    Expect-Compile-Error 'extern_signature_error' "argument 1 of 'mira_abs': expected f64, got i64"
    Expect-Compile-Error 'typed_postfix_insufficient_error' "function 'sum' expects 2 arguments, got 1"
    Expect-Compile-Error 'call_question_arity_error' "expects 1 arguments, got 2"
    Expect-Compile-Error 'method_arity_error' "expects 1 arguments, got 2"
    Expect-Compile-Error 'void_return_branch_origin_error' "function 'finish': expected void, got i64" 'Line 5, Column 9'

    Push-Location $types
    try {
        & $Mira -O0 'extern_signature_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'extern_signature_valid O0 compile failed' }
        $actual = ((& (Get-BinaryPath $types 'extern_signature_valid')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'extern_signature_valid O0 run failed' }
        if ($actual -ne '42') { throw "extern_signature_valid output '$actual', expected '42'" }
    } finally {
        Pop-Location
    }

    Push-Location $types
    try {
        & $Mira -O0 'legacy_nested_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'legacy_nested_valid O0 compile failed' }

        foreach ($postfixCase in @(
            @{ Name = 'typed_postfix_exact_valid'; Expected = '42' },
            @{ Name = 'typed_postfix_prefix_value_valid'; Expected = '142' },
            @{ Name = 'legacy_postfix_valid'; Expected = '7' }
        )) {
            & $Mira -O0 "$($postfixCase.Name).mira" | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "$($postfixCase.Name) O0 compile failed" }
            $actual = ((& (Get-BinaryPath $types $postfixCase.Name)) -join "`n").Trim()
            if ($LASTEXITCODE -ne 0) { throw "$($postfixCase.Name) O0 run failed" }
            if ($actual -ne $postfixCase.Expected) {
                throw "$($postfixCase.Name) output '$actual', expected '$($postfixCase.Expected)'"
            }
        }

        & $Mira -O0 'branch_all_paths_return_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'branch_all_paths_return_valid O0 compile failed' }
        $actual = ((& (Get-BinaryPath $types 'branch_all_paths_return_valid')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'branch_all_paths_return_valid O0 run failed' }
        if ($actual -ne '1') { throw "branch_all_paths_return_valid output '$actual', expected '1'" }

        & $Mira -O0 'try_all_paths_return_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'try_all_paths_return_valid O0 compile failed' }
        $actual = ((& (Get-BinaryPath $types 'try_all_paths_return_valid')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'try_all_paths_return_valid O0 run failed' }
        if ($actual -ne '1') { throw "try_all_paths_return_valid output '$actual', expected '1'" }

        & $Mira -O0 'method_arity_valid.mira' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'method_arity_valid O0 compile failed' }
        $actual = ((& (Get-BinaryPath $types 'method_arity_valid')) -join "`n").Trim()
        if ($LASTEXITCODE -ne 0) { throw 'method_arity_valid O0 run failed' }
        if ($actual -ne '42') { throw "method_arity_valid output '$actual', expected '42'" }
    } finally {
        Pop-Location
    }

    Write-Output 'GRADUAL TYPE CALLS PASS'
}

if ($Group -in @('all', 'expressions')) {
    Expect-Compile-Error 'assignment_type_error' 'assignment to count: expected i64, got str'
    Expect-Compile-Error 'mixed_numeric_error' 'operator +: expected matching numeric types, got i64 and f64'
    Expect-Compile-Error 'condition_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'branch_type_error' "function 'choose': expected i64, got str"
    Expect-Compile-Error 'branch_operator_type_error' 'operator +: expected numeric type, got str' 'Line 2,'
    Expect-Compile-Error 'postfix_and_type_error' 'operator and: expected bool, got i64'
    Expect-Compile-Error 'postfix_or_type_error' 'operator or: expected bool, got i64'
    Expect-Compile-Error 'postfix_xor_type_error' 'operator xor: expected bool, got i64'
    Expect-Compile-Error 'postfix_not_type_error' 'operator not: expected bool, got i64'
    Expect-Compile-Error 'neg_string_type_error' 'operator neg: expected i64, got str'
    Expect-Compile-Error 'neg_float_type_error' 'operator neg: expected i64, got f64'
    Expect-Compile-Error 'while_one_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'while_zero_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'ordinary_if_shape_type_error' "function 'choose': expected bool, got i64"
    Expect-Compile-Error 'logical_and_left_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'logical_and_right_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'logical_or_left_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'logical_or_right_type_error' 'condition: expected bool, got i64'
    Expect-Compile-Error 'ptr_string_equality_type_error' 'expected matching types, got ptr and str'
    Expect-Compile-Error 'ptr_i64_equality_type_error' 'expected matching types, got ptr and i64'
    Expect-Compile-Error 'ptr_string_inequality_type_error' 'expected matching types, got ptr and str'
    Expect-Compile-Error 'ptr_ordered_type_error' 'expected numeric type, got ptr'
    Expect-Compile-Error 'str_i64_ordered_type_error' 'expected numeric type, got str'

    Push-Location $types
    try {
        foreach ($expressionCase in @(
            @{ Name = 'scalars_valid'; Expected = '7' },
            @{ Name = 'legacy_truthiness_valid'; Expected = '7' },
            @{ Name = 'postfix_bool_valid'; Expected = '-2' },
            @{ Name = 'operator_matrix_valid'; Expected = "-5`n1" },
            @{ Name = 'while_true_valid'; Expected = '7' },
            @{ Name = 'legacy_while_one_valid'; Expected = '7' },
            @{ Name = 'dynamic_conditions_valid'; Expected = '2' },
            @{ Name = 'comparison_types_valid'; Expected = "1`n1`n1`n1" }
        )) {
            & $Mira -O0 "$($expressionCase.Name).mira" | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "$($expressionCase.Name) O0 compile failed" }
            $actual = ((& (Get-BinaryPath $types $expressionCase.Name)) -join "`n").Trim()
            if ($LASTEXITCODE -ne 0) { throw "$($expressionCase.Name) O0 run failed" }
            if ($actual -ne $expressionCase.Expected) {
                throw "$($expressionCase.Name) output '$actual', expected '$($expressionCase.Expected)'"
            }
        }
    } finally {
        Pop-Location
    }

    Write-Output 'GRADUAL TYPE EXPRESSIONS PASS'
}

if ($Group -in @('all', 'ssa')) {
    $expected = "2.5`n2.5`n3.5`n7.5`ntyped`n1`nvoid"

    Push-Location $types
    try {
        foreach ($level in 0..3) {
            & $Mira "-O$level" 'ssa_typed_values.mira' | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "ssa_typed_values O$level compile failed" }
            $actual = ((& (Get-BinaryPath $types 'ssa_typed_values')) -join "`n").Trim()
            if ($LASTEXITCODE -ne 0) { throw "ssa_typed_values O$level run failed: $LASTEXITCODE" }
            if ($actual -ne $expected) {
                throw "ssa_typed_values O$level output '$actual', expected '$expected'"
            }
        }

        $focusedCases = @(
            @{ Name = 'legacy_unannotated_mut_valid'; Expected = 'typed' },
            @{ Name = 'f64_comparisons_valid'; Expected = "0`n1`n1`n1`n1`n1`n1`n0`n0`n1`n0`n0`n0`n0" },
            @{ Name = 'if_typed_phi_valid'; Expected = "1.5`n2.5`nleft`nright`n3.5`ndirect-right`n11`n22" },
            @{ Name = 'if_owned_string_phi_valid'; Expected = "mira`ntyped`nmixed`nborrowed" },
            @{ Name = 'switch_try_tail_values_valid'; Expected = "11`n22`n11`n1.25`n2.5`nswitch-left`nswitch-right`n3.5`ntry-normal" }
        )
        foreach ($case in $focusedCases) {
            foreach ($level in @(0, 3)) {
                & $Mira "-O$level" "$($case.Name).mira" | Out-Host
                if ($LASTEXITCODE -ne 0) { throw "$($case.Name) O$level compile failed" }
                $actual = ((& (Get-BinaryPath $types $case.Name)) -join "`n").Trim()
                if ($LASTEXITCODE -ne 0) { throw "$($case.Name) O$level run failed: $LASTEXITCODE" }
                if ($actual -ne $case.Expected) {
                    throw "$($case.Name) O$level output '$actual', expected '$($case.Expected)'"
                }
            }
        }

        $irPath = Join-Path $types 'ssa_typed_values.O0.ir'
        & $Mira -S 'ssa_typed_values.mira' $irPath -O0 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'ssa_typed_values O0 IR dump failed' }
        $ir = Get-Content -Raw -LiteralPath $irPath

        $floatCall = [regex]::Match($ir,
            '(?ms)^mira_main:\r?\n.*?call typed_float\r?\n(?<after>.*?)(?=^\s*call typed_string\r?$)')
        if (-not $floatCall.Success -or
            $floatCall.Groups['after'].Value -notmatch '(?m)^\s*movq [xy]mm[0-9]+, rax\r?$') {
            throw 'typed_float call result is not lowered through the float register class'
        }

        $floatBody = [regex]::Match($ir,
            '(?ms)^typed_float:\r?\n(?<body>.*?)(?=^[A-Za-z_][A-Za-z0-9_]*:\r?$|\z)')
        if (-not $floatBody.Success -or
            $floatBody.Groups['body'].Value -notmatch '(?m)^\s*movq r(?:ax|10|11), [xy]mm[0-9]+\r?$') {
            throw 'typed_float return value is not lowered from the float register class'
        }

        $secondParamBody = [regex]::Match($ir,
            '(?ms)^pick_second:\r?\n(?<body>.*?)(?=^[A-Za-z_][A-Za-z0-9_]*:\r?$|\z)')
        if (-not $secondParamBody.Success -or
            $secondParamBody.Groups['body'].Value -notmatch
                '(?m)^\s*mov r(?<scratch>10|11), \[rbp \+ -[1-9][0-9]*\]\r?\n\s*movq [xy]mm[0-9]+, r\k<scratch>\r?$') {
            throw 'pick_second does not load its second f64 parameter from a SysV register-parameter home slot into the float register class'
        }

        $mixedParamBody = [regex]::Match($ir,
            '(?ms)^pick_mixed:\r?\n(?<body>.*?)(?=^[A-Za-z_][A-Za-z0-9_]*:\r?$|\z)')
        if (-not $mixedParamBody.Success -or
            $mixedParamBody.Groups['body'].Value -notmatch
                '(?m)^\s*mov r(?<scratch>10|11), \[rbp \+ -[1-9][0-9]*\]\r?\n\s*movq [xy]mm[0-9]+, r\k<scratch>\r?$') {
            throw 'pick_mixed does not preserve the f64 type and SysV home-slot location of its second parameter'
        }

        $seventhParamBody = [regex]::Match($ir,
            '(?ms)^pick_seventh:\r?\n(?<body>.*?)(?=^[A-Za-z_][A-Za-z0-9_]*:\r?$|\z)')
        if (-not $seventhParamBody.Success -or
            $seventhParamBody.Groups['body'].Value -notmatch
                '(?m)^\s*mov r(?<scratch>10|11), \[rbp \+ 64\]\r?\n\s*movq [xy]mm[0-9]+, r\k<scratch>\r?$') {
            throw 'pick_seventh does not load its f64 overflow parameter from the SysV caller stack area'
        }

        $stringPrint = [regex]::Match($ir,
            '(?ms)^mira_main:\r?\n.*?call typed_bool\r?\n.*?call mira_print\r?\n.*?call typed_string\r?\n(?<after>.*?call mira_print\r?$)')
        if (-not $stringPrint.Success -or
            $stringPrint.Groups['after'].Value -notmatch '(?m)^\s*mov rax, 1\r?$') {
            throw 'typed_string call result is not printed with the pointer type tag'
        }

        $pointerPrint = [regex]::Match($ir,
            '(?ms)call typed_pointer\r?\n\s*mov (?<pointer>r(?:1[0-5]|[8-9]|ax|bx|cx|dx|si|di)), rax\r?\n.*?' +
            'mov \[rsp \+ 8\], \k<pointer>\r?\n\s*mov rax, 1\r?\n.*?call mira_print\r?$')
        if (-not $pointerPrint.Success) {
            throw 'typed_pointer call result is not lowered and printed with the pointer type tag'
        }

        $voidCall = [regex]::Match($ir,
            '(?ms)^mira_main:\r?\n.*?call typed_void\r?\n(?<after>.*?)(?=^\s*ret\r?$)')
        if (-not $voidCall.Success -or
            $voidCall.Groups['after'].Value -match '(?m)^\s*mov (?!rsp\b)[^,]+, rax\r?$') {
            throw 'typed_void call unexpectedly materializes an integer result'
        }
    } finally {
        Remove-Item -LiteralPath (Join-Path $types 'ssa_typed_values.O0.ir') -ErrorAction SilentlyContinue
        Pop-Location
    }

    Write-Output 'GRADUAL TYPE SSA PASS'
}
