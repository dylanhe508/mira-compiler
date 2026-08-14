param(
    [string]$Mira = '',
    [string]$OldMira = '',
    [string]$Gcc = 'gcc'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$oldMiraRequested = [bool]$OldMira
if (-not $OldMira) { $OldMira = Join-Path $root 'mira-phi-cfg.exe' }
$hasOldMira = Test-Path -LiteralPath $OldMira
if ($oldMiraRequested -and -not $hasOldMira) {
    throw "explicit historical compiler not found: $OldMira"
}

$source = Join-Path $PSScriptRoot 'regression_nonvolatile_calls.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_nonvolatile_calls.c'
$oracleExe = Join-Path $PSScriptRoot 'regression_nonvolatile_calls_oracle.exe'
$miraExe = Join-Path $root 'regression_nonvolatile_calls.exe'
$recursionSource = Join-Path $PSScriptRoot 'regression_nonvolatile_recursion.mira'
$recursionOracleSource = Join-Path $PSScriptRoot 'regression_nonvolatile_recursion.c'
$recursionOracleExe = Join-Path $PSScriptRoot 'regression_nonvolatile_recursion_oracle.exe'
$recursionExe = Join-Path $root 'regression_nonvolatile_recursion.exe'
$stackOverflowExit = [int64]3221225725 # 0xC00000FD
$runtime = @(
    (Join-Path $root 'runtime\rt_core.obj'),
    (Join-Path $root 'runtime\rt_print.obj'),
    (Join-Path $root 'runtime\rt_win.obj'),
    (Join-Path $root 'runtime\rt_mem.obj')
)

foreach ($path in @(
        $Mira, $source, $oracleSource,
        $recursionSource, $recursionOracleSource) + $runtime) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "missing required file: $path"
    }
}

function Invoke-Program([string]$Exe) {
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $text = (& $Exe 2>&1 | Out-String)
        $exitCode = [int64]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return @{ Text = $text; ExitCode = $exitCode }
}

function Normalize-Output([string]$Exe) {
    $result = Invoke-Program $Exe
    if ($result.ExitCode -ne 0) {
        throw "program execution failed exit=$($result.ExitCode)"
    }
    return (($result.Text -split '\s+' | Where-Object { $_ }) -join ',')
}

function Get-FunctionText([string]$AsmPath, [string]$FunctionName) {
    $lines = Get-Content -LiteralPath $AsmPath
    $start = -1
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i].Trim() -eq "${FunctionName}:") {
            $start = $i
            break
        }
    }
    if ($start -lt 0) { throw "function $FunctionName not found in $AsmPath" }
    $end = $lines.Count
    for ($i = $start + 1; $i -lt $lines.Count; ++$i) {
        if ($lines[$i] -match '^[A-Za-z_][A-Za-z0-9_]*:$') {
            $end = $i
            break
        }
    }
    return ($lines[$start..($end - 1)] -join "`n")
}

function Compile-And-Run([string]$Compiler, [string]$Tag, [int]$Level,
                         [string]$SourcePath, [string]$Output, [string]$Expected) {
    & $Compiler "-O$Level" $SourcePath
    if ($LASTEXITCODE -ne 0) { throw "$Tag O$Level compile failed" }
    $obj = Join-Path $root ('out\' +
        [IO.Path]::GetFileNameWithoutExtension($SourcePath) + '.obj')
    if (-not (Test-Path -LiteralPath $obj)) {
        throw "$Tag O$Level did not produce $obj"
    }
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $Output
    if ($LASTEXITCODE -ne 0) { throw "$Tag O$Level GCC link failed" }
    if (-not (Test-Path -LiteralPath $Output)) {
        throw "$Tag O$Level did not produce $Output"
    }
    $actual = Normalize-Output $Output
    if ($actual -ne $Expected) {
        throw "$Tag O$Level expected=$Expected actual=$actual"
    }
    Write-Output "$Tag O$Level PASS $actual"
}

function Assert-OldRecursionStackOverflow([string]$Compiler, [string]$Tag) {
    & $Compiler '-O3' $recursionSource
    if ($LASTEXITCODE -ne 0) { throw "$Tag O3 recursion compile failed" }
    if (-not (Test-Path -LiteralPath $recursionExe)) {
        throw "$Tag O3 recursion did not produce $recursionExe"
    }
    $result = Invoke-Program $recursionExe
    $unsignedExit = $result.ExitCode -band [int64]4294967295
    if ($result.ExitCode -eq 0) {
        throw "$Tag O3 recursion unexpectedly succeeded; expected stack overflow 0xC00000FD"
    }
    if ($unsignedExit -ne $stackOverflowExit) {
        throw "$Tag O3 recursion expected stack overflow 0xC00000FD exit=0x$('{0:X8}' -f $unsignedExit)"
    }
    Write-Output "$Tag O3 KNOWN-FAIL recursion stack-overflow exit=0xC00000FD"
}

function Dump-Shape([string]$Compiler, [string]$Tag, [int]$Level) {
    $asm = Join-Path $root "nonvolatile_${Tag}_O${Level}.asm"
    & $Compiler -S $source $asm "-O$Level"
    if ($LASTEXITCODE -ne 0) { throw "$Tag O$level assembly dump failed" }
    $body = Get-FunctionText $asm 'ssa_shape'
    $spills = [regex]::Matches(
        $body, '(?i)\[rbp\s*(?:\+\s*)?-\s*\d+\]').Count
    $pushes = [regex]::Matches(
        $body, '(?im)^\s*push\s+(r13|r14|r15|rbx|rdi|rsi|r12)\s*$').Count
    $pops = [regex]::Matches(
        $body, '(?im)^\s*pop\s+(r13|r14|r15|rbx|rdi|rsi|r12)\s*$').Count
    return @{ Asm = $asm; Spills = $spills; Pushes = $pushes; Pops = $pops }
}

$generated = @($oracleExe, $miraExe, $recursionOracleExe, $recursionExe)
try {
    & $Gcc -std=c11 -O2 $oracleSource -o $oracleExe
    if ($LASTEXITCODE -ne 0) { throw 'main C oracle build failed' }
    $oracle = Normalize-Output $oracleExe
    Write-Output "main oracle PASS $oracle"

    & $Gcc -std=c11 -O2 $recursionOracleSource -o $recursionOracleExe
    if ($LASTEXITCODE -ne 0) { throw 'recursion C oracle build failed' }
    $recursionOracle = Normalize-Output $recursionOracleExe
    Write-Output "recursion oracle PASS $recursionOracle"

    if ($hasOldMira) {
        Compile-And-Run $OldMira 'old main' 0 $source $miraExe $oracle
        foreach ($level in 0..2) {
            Compile-And-Run $OldMira 'old recursion' $level $recursionSource $recursionExe $recursionOracle
        }
        Assert-OldRecursionStackOverflow $OldMira 'old'
    }
    else {
        Write-Output "historical compiler unavailable; running strict current-only checks"
    }
    foreach ($level in 0..2) {
        Compile-And-Run $Mira 'new recursion' $level $recursionSource $recursionExe $recursionOracle
    }
    Compile-And-Run $Mira 'new recursion' 3 $recursionSource $recursionExe $recursionOracle

    $oldShapes = @{}
    if ($hasOldMira) {
        foreach ($level in 2..3) {
            $shape = Dump-Shape $OldMira 'old' $level
            $oldShapes[$level] = $shape
            $generated += $shape.Asm
            if ($shape.Spills -ne 2 -or $shape.Pushes -ne 0) {
                throw "old O$level expected spills=2 nonvolatile_pushes=0; got spills=$($shape.Spills) pushes=$($shape.Pushes)"
            }
            Write-Output "old O$level SHAPE spills=$($shape.Spills) nonvolatile_pushes=$($shape.Pushes)"
        }
    }

    foreach ($level in 0..3) {
        Compile-And-Run $Mira 'new main' $level $source $miraExe $oracle
    }

    foreach ($level in 2..3) {
        $shape = Dump-Shape $Mira 'new' $level
        $generated += $shape.Asm
        if ($shape.Pushes -le 0 -or $shape.Pushes -ne $shape.Pops) {
            throw "new O$level expected balanced nonvolatile push/pop"
        }
        if ($hasOldMira -and $shape.Spills -ge $oldShapes[$level].Spills) {
            throw "new O$level spills=$($shape.Spills) old=$($oldShapes[$level].Spills)"
        }
        if (-not $hasOldMira -and $shape.Spills -ne 0) {
            throw "new O$level expected spills=0 in current-only mode; got spills=$($shape.Spills)"
        }
        Write-Output "new O$level SHAPE spills=$($shape.Spills) nonvolatile_pushes=$($shape.Pushes) nonvolatile_pops=$($shape.Pops)"
    }
}
finally {
    foreach ($path in $generated | Select-Object -Unique) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}
