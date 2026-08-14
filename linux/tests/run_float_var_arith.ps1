$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Mira = Join-Path $Root 'mira.exe'
$Source = Join-Path $Root 'tests\regression_float_var_arith.mira'
$Exe = Join-Path $Root 'regression_float_var_arith.exe'

foreach ($opt in @('O0', 'O1', 'O2', 'O3')) {
    Remove-Item -LiteralPath $Exe -ErrorAction SilentlyContinue
    Push-Location $Root
    try {
        & $Mira "-$opt" $Source | Out-Null
        $output = @(& $Exe)
    } finally {
        Pop-Location
    }

    if ($output.Count -ne 2) {
        throw "$opt expected 2 output lines, got $($output.Count): $($output -join ',')"
    }
    if ($output[0] -ne '1') {
        throw "$opt single variable float division expected 1, got $($output[0])"
    }
    if ($output[1] -ne '1.12751') {
        throw "$opt loop variable float division expected 1.12751, got $($output[1])"
    }
}

Write-Output 'FLOAT VAR ARITH PASS'
