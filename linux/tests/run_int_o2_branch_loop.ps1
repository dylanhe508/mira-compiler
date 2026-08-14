$ErrorActionPreference = 'Stop'

$WinDir = Split-Path -Parent $PSScriptRoot
$Source = Join-Path $PSScriptRoot 'regression_int_o2_branch_loop.mira'
$Exe = Join-Path $WinDir 'regression_int_o2_branch_loop.exe'
$Expected = '1236698126'

Push-Location $WinDir
try {
    foreach ($opt in @('O0', 'O1', 'O2', 'O3')) {
        Remove-Item -LiteralPath $Exe -ErrorAction SilentlyContinue
        & (Join-Path $WinDir 'mira.exe') "-$opt" $Source | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Mira $opt build failed"
        }
        $Actual = (& $Exe | Select-Object -First 1)
        if ($LASTEXITCODE -ne 0) {
            throw "Mira $opt run failed"
        }
        if ($Actual -ne $Expected) {
            throw "FAIL $opt expected=$Expected actual=$Actual"
        }
    }
}
finally {
    Pop-Location
}

Write-Output 'INT O2 BRANCH LOOP PASS'
