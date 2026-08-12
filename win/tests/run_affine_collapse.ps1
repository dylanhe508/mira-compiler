param([string]$Mira = '', [string]$Gcc = 'gcc')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$out = Join-Path $PSScriptRoot 'regression-out\affine-collapse'
[void][IO.Directory]::CreateDirectory($out)
$source = Join-Path $out 'affine_collapse_case.mira'
$oracleSource = Join-Path $out 'affine_collapse_case.c'

@'
fn pressure(x) {
    mut a = x + 1;
    mut b = x * 3 + 2;
    mut c = x * 5 + 3;
    mut d = x * 7 + 4;
    mut e = x * 11 + 5;
    mut f = x * 13 + 6;
    mut g = x * 17 + 7;
    mut h = x * 19 + 8;
    mut i = x * 23 + 9;
    mut j = x * 29 + 10;
    mut k = x * 31 + 11;
    mut l = (x * 37 + 12) & 9223372036854775807;
    (a * 3 + b * 5 + c * 7 + d * 11 + e * 13 + f * 17 +
     g * 19 + h * 23 + i * 29 + j * 31 + k * 37 + l * 41) &
        9223372036854775807
}

fn main() {
    mut total = 0;
    mut n = 0;
    while (n < 10000) {
        total = (total + pressure(n + 24680)) & 9223372036854775807;
        n = n + 1;
    }
    print(total);
}
'@ | Set-Content -LiteralPath $source -Encoding ascii

@'
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t pressure(uint64_t x) {
    uint64_t a=x+1, b=x*3+2, c=x*5+3, d=x*7+4;
    uint64_t e=x*11+5, f=x*13+6, g=x*17+7, h=x*19+8;
    uint64_t i=x*23+9, j=x*29+10, k=x*31+11;
    uint64_t l=(x*37+12)&INT64_MAX;
    return (a*3+b*5+c*7+d*11+e*13+f*17+g*19+h*23+i*29+
            j*31+k*37+l*41)&INT64_MAX;
}

int main(void) {
    uint64_t total = 0;
    for (uint64_t n = 0; n < 10000; ++n)
        total = (total + pressure(n + 24680)) & INT64_MAX;
    printf("%" PRIu64 "\n", total);
    return 0;
}
'@ | Set-Content -LiteralPath $oracleSource -Encoding ascii

foreach ($path in @($Mira, $source, $oracleSource)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "missing: $path" }
}

$oracle = Join-Path $out 'oracle.exe'
& $Gcc -std=c11 -O2 $oracleSource -o $oracle
if ($LASTEXITCODE -ne 0) { throw 'oracle build failed' }
$expected = ((& $oracle) -join ',').Trim()

foreach ($level in 0..3) {
    Push-Location $root
    try { & $Mira "-O$level" $source | Out-Host }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Mira O$level compile failed" }
    $exe = Join-Path $root 'affine_collapse_case.exe'
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
if ($LASTEXITCODE -ne 0) { throw 'enabled IR compile failed' }
$env:MIRA_DECISION_DISABLE = 'affine-collapse'
try { & $Mira -S $source $disabledAsm -O3 | Out-Host }
finally { Remove-Item Env:MIRA_DECISION_DISABLE -ErrorAction SilentlyContinue }
if ($LASTEXITCODE -ne 0) { throw 'disabled IR compile failed' }
$enabled = Count-Multiply $enabledAsm
$disabled = Count-Multiply $disabledAsm
if ($enabled -gt 1 -or $disabled -le 1) {
    throw "shape mismatch enabled_imul=$enabled disabled_imul=$disabled"
}
Write-Output "SHAPE PASS enabled_imul=$enabled disabled_imul=$disabled"
