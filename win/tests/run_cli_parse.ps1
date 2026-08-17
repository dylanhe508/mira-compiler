$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$testSource = Join-Path $PSScriptRoot 'cli_parse_test.c'
$testExe = Join-Path $PSScriptRoot 'cli_parse_test.exe'

try {
    & gcc -std=c11 -Wall -Wextra -Werror -I $root $testSource (Join-Path $root 'cli.c') -o $testExe
    if ($LASTEXITCODE -ne 0) { throw 'CLI parser test build failed' }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'CLI parser test failed' }
} finally {
    Remove-Item -LiteralPath $testExe -ErrorAction SilentlyContinue
}
