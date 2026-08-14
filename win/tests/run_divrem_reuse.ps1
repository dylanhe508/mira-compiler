param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }

$source = Join-Path $PSScriptRoot 'regression_divrem_reuse.mira'
$shapeSource = Join-Path $PSScriptRoot 'regression_divrem_shape.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_divrem_reuse.c'
$out = Join-Path $PSScriptRoot 'regression-out\divrem-reuse'
$oracle = Join-Path $out 'regression_divrem_reuse_oracle.exe'

foreach ($path in @($Mira, $source, $shapeSource, $oracleSource)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing required file: $path" }
}

$runtime = @(
    (Join-Path $root 'runtime\rt_core.obj'),
    (Join-Path $root 'runtime\rt_print.obj'),
    (Join-Path $root 'runtime\rt_win.obj'),
    (Join-Path $root 'runtime\rt_mem.obj')
)
foreach ($path in $runtime) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing Mira runtime object: $path" }
}

[void][IO.Directory]::CreateDirectory($out)
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'GCC oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()
if ($LASTEXITCODE -ne 0) { throw 'GCC oracle run failed' }
Write-Output "oracle $expected"

function Get-ShapeSignedDivisionCount([int]$level, [bool]$disableMagic) {
    $savedDisable = $env:MIRA_DECISION_DISABLE
    try {
        if ($disableMagic) { $env:MIRA_DECISION_DISABLE = 'magic' }
        else { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }

        Push-Location $root
        try {
            $shapeDump = (& $Mira "-O$level" $shapeSource 2>&1 | Out-String)
            $shapeExitCode = $LASTEXITCODE
        }
        finally {
            Pop-Location
        }
        if ($shapeExitCode -ne 0) {
            throw "Mira O$level shape fixture object build failed"
        }
        $shapeObj = Join-Path $root 'out\regression_divrem_shape.obj'
        if (-not (Test-Path -LiteralPath $shapeObj)) {
            throw "Mira O$level did not produce $shapeObj"
        }
        $disassembly = (& objdump -d $shapeObj 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0) { throw "objdump failed for Mira O$level shape fixture" }
        return [regex]::Matches($disassembly, '(?i)\bidiv\s+').Count
    }
    finally {
        if ($null -eq $savedDisable) {
            Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue
        }
        else { $env:MIRA_DECISION_DISABLE = $savedDisable }
    }
}

foreach ($level in 0..3) {
    Push-Location $root
    try {
        $dump = (& $Mira "-O$level" $source 2>&1 | Out-String)
    }
    finally {
        Pop-Location
    }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level object build failed" }

    $obj = Join-Path $root 'out\regression_divrem_reuse.obj'
    if (-not (Test-Path -LiteralPath $obj)) { throw "Mira O$level did not produce $obj" }
    $exe = Join-Path $out "regression_divrem_reuse_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level standard-runtime link failed" }

    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level run failed" }
    if ($actual -ne $expected) {
        throw "Mira O$level mismatch expected=$expected actual=$actual"
    }

    Write-Output "O$level PASS $actual"
}

foreach ($level in 2..3) {
    $disabledCount = Get-ShapeSignedDivisionCount $level $true
    if ($disabledCount -ne 2) {
        throw "Mira O$level expected two signed divisions in the disabled shape fixture program, found $disabledCount"
    }
    $enabledCount = Get-ShapeSignedDivisionCount $level $false
    if ($enabledCount -ne 1) {
        throw "Mira O$level expected one signed division in the enabled shape fixture program, found $enabledCount"
    }
    Write-Output "O$level shape fixture PASS disabled-signed-divisions=$disabledCount enabled-signed-divisions=$enabledCount"
}
