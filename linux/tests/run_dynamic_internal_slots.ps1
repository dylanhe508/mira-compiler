param(
    [string]$Mira = '',
    [string]$Gcc = 'gcc'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$source = Join-Path $PSScriptRoot 'regression_dynamic_internal_slots.mira'
$oracleSource = Join-Path $PSScriptRoot 'regression_dynamic_internal_slots.c'
$out = Join-Path $PSScriptRoot 'regression-out\dynamic-internal-slots'
$runtime = @(
    (Join-Path $root 'runtime\rt_core.obj'),
    (Join-Path $root 'runtime\rt_print.obj'),
    (Join-Path $root 'runtime\rt_win.obj'),
    (Join-Path $root 'runtime\rt_mem.obj')
)
foreach ($path in @($Mira, $source, $oracleSource) + $runtime) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing required file: $path" }
}
[void][IO.Directory]::CreateDirectory($out)

$oracle = Join-Path $out 'oracle.exe'
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()
if ($LASTEXITCODE -ne 0) { throw 'oracle run failed' }

foreach ($level in 0..3) {
    Push-Location $root
    try { & $Mira "-O$level" $source | Out-Host }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }
    $obj = Join-Path $root 'out\regression_dynamic_internal_slots.obj'
    $exe = Join-Path $out "dynamic_slots_O$level.exe"
    & $Gcc $obj $runtime '-Wl,--allow-multiple-definition' -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level link failed" }
    $actual = ((& $exe) -join ',').Trim()
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level run failed" }
    if ($actual -ne $expected) {
        throw "Mira O$level mismatch expected=$expected actual=$actual"
    }
    Write-Output "O$level PASS $actual"
}

function Count-Loop-Multiply([string]$Asm) {
    $lines = Get-Content -LiteralPath $Asm
    $count = 0
    foreach ($number in 0..19) {
        $start = [array]::IndexOf($lines, "slot_${number}:")
        if ($start -lt 0) { throw "missing slot_${number} in $Asm" }
        $end = $lines.Count
        for ($i = $start + 1; $i -lt $lines.Count; ++$i) {
            if ($lines[$i] -match '^[A-Za-z_][A-Za-z0-9_]*:$') {
                $end = $i
                break
            }
        }
        $body = $lines[$start..($end - 1)] -join "`n"
        if ($body -match '(?im)^\s*imul\s') { ++$count }
    }
    return $count
}

$enabledAsm = Join-Path $out 'enabled.asm'
$disabledAsm = Join-Path $out 'disabled.asm'
& $Mira -S $source -o $enabledAsm -O3 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'enabled assembly build failed' }
$env:MIRA_DECISION_DISABLE = 'affine'
try { & $Mira -S $source -o $disabledAsm -O3 | Out-Host }
finally { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }
if ($LASTEXITCODE -ne 0) { throw 'disabled assembly build failed' }
$enabled = Count-Loop-Multiply $enabledAsm
$disabled = Count-Loop-Multiply $disabledAsm
if ($enabled -ne 0 -or $disabled -ne 20) {
    throw "shape mismatch enabled_imul=$enabled disabled_imul=$disabled"
}
$lines = Get-Content -LiteralPath $enabledAsm
$reservation = ($lines | Select-String '^mira_vars:$' -Context 0,1).Context.PostContext[0].Trim()
if ($reservation -ne '.zero 648') { throw "expected .zero 648, got $reservation" }
Write-Output "SHAPE PASS enabled_imul=0 disabled_imul=20 bss=$reservation"
