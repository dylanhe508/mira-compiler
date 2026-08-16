param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_cross_branch_affine.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_cross_branch_affine.c'
$out = Join-Path $PSScriptRoot 'regression-out\cross-branch-affine'
$runtime = @('rt_core.obj','rt_print.obj','rt_win.obj','rt_mem.obj') |
    ForEach-Object { Join-Path $root "runtime\$_" }
foreach ($path in @($Mira, $source, $oracleSource) + $runtime) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing: $path" }
}
[void][IO.Directory]::CreateDirectory($out)
$oracle = Join-Path $out 'oracle.exe'
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()

foreach ($level in 0..3) {
    Push-Location $root
    try { & $Mira "-O$level" $source | Out-Host }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }
    $obj = Join-Path $root 'out\regression_cross_branch_affine.obj'
    $exe = Join-Path $out "cross_branch_affine_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) {
        throw "O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}

function Count-Multiply([string]$Asm) {
    @((Get-Content -LiteralPath $Asm) | Select-String '(?i)^\s*imul\s').Count
}
$enabledAsm = Join-Path $out 'enabled.asm'
$disabledAsm = Join-Path $out 'disabled.asm'
& $Mira -S $source $enabledAsm -O3 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'enabled assembly build failed' }
$env:MIRA_DECISION_DISABLE = 'affine'
try { & $Mira -S $source $disabledAsm -O3 | Out-Host }
finally { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }
if ($LASTEXITCODE -ne 0) { throw 'disabled assembly build failed' }
$enabled = Count-Multiply $enabledAsm
$disabled = Count-Multiply $disabledAsm
if ($enabled -ge $disabled) {
    throw "branch recurrence missing enabled_imul=$enabled disabled_imul=$disabled"
}
$lines = Get-Content -LiteralPath $enabledAsm
$start = ($lines | Select-String '^branchy_recurrence:').LineNumber
$finish = ($lines | Select-String '^mira_main:').LineNumber
$body = $lines[($start - 1)..($finish - 2)]
$spills = @($body | Select-String '\[rbp \+ -').Count
if ($spills -ne 0) {
    throw "branch selector spilled after recurrence spills=$spills"
}
Write-Output "SHAPE PASS enabled_imul=$enabled disabled_imul=$disabled spills=0"
