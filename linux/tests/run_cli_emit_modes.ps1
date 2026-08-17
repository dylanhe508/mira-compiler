param(
    [ValidateSet('pipeline', 'ir-dump', 'asm', 'artifacts', 'all')]
    [string]$Group = 'all',
    [string]$Mira = ''
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Mira) { $Mira = Join-Path $root 'mira.exe' }
$fixture = Join-Path $PSScriptRoot 'final_ir_pipeline.mira'
$outDir = Join-Path $PSScriptRoot 'regression-out\cli-emit-modes'
$irPath = Join-Path $outDir 'final_ir_pipeline.ir'
$irDumpTest = Join-Path $outDir 'ir_dump_test.exe'
$irDumpSink = Join-Path $outDir 'invalid.ir'
$asmWriterTest = Join-Path $outDir 'asm_writer_test.exe'
$asmFixture = Join-Path $PSScriptRoot 'asm_emit_full.mira'
$shortAsm = Join-Path $outDir 'short.s'
$longAsm = Join-Path $outDir 'long.s'
$asmObject = Join-Path $outDir 'short.obj'

try {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    if ($Group -in @('pipeline', 'all')) {
        & $Mira --emit=ir $fixture -o $irPath -O3 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw '--emit=ir failed' }
        if (!(Test-Path -LiteralPath $irPath)) { throw '--emit=ir did not create output' }
        $ir = Get-Content -Raw -LiteralPath $irPath
        if ($ir -notmatch '^;; Mira IR dump') { throw '--emit=ir did not write Mira IR' }
        if ($ir -match '(?m)^\s*add\s+[^,]+,\s*0\s*$') {
            throw 'late add-zero cleanup did not reach emitted IR'
        }
        Write-Output 'FINAL IR PIPELINE PASS'
    }
    if ($Group -in @('ir-dump', 'all')) {
        & gcc -std=c11 -Wall -Wextra -Werror -I $root `
            (Join-Path $PSScriptRoot 'ir_dump_test.c') `
            (Join-Path $root 'codegen\ir_dump.c') -o $irDumpTest
        if ($LASTEXITCODE -ne 0) { throw 'ir_dump_test build failed' }
        & $irDumpTest $irDumpSink | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'ir_dump_test failed' }
    }
    if ($Group -in @('asm', 'all')) {
        & gcc -std=c11 -Wall -Wextra -Werror -I $root `
            (Join-Path $PSScriptRoot 'asm_writer_test.c') `
            (Join-Path $root 'codegen\asm_writer.c') -o $asmWriterTest
        if ($LASTEXITCODE -ne 0) { throw 'asm_writer_test build failed' }
        & $asmWriterTest | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'asm_writer_test failed' }
        & $Mira -S $asmFixture -o $shortAsm -O3 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw '-S assembly emission failed' }
        & $Mira --emit=asm $asmFixture -o $longAsm -O3 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw '--emit=asm assembly emission failed' }
        if ((Get-FileHash $shortAsm).Hash -ne (Get-FileHash $longAsm).Hash) {
            throw '-S and --emit=asm outputs differ'
        }
        & gcc -c $shortAsm -o $asmObject
        if ($LASTEXITCODE -ne 0) { throw 'GNU assembler rejected emitted assembly' }
        $runtimeObjects = @('rt_core.obj', 'rt_print.obj', 'rt_mem.obj') |
            ForEach-Object { Join-Path $root "runtime\$_" }
        if (($runtimeObjects | Where-Object { -not (Test-Path -LiteralPath $_) }).Count -eq 0) {
            $asmExe = Join-Path $outDir 'short.exe'
            & gcc $asmObject $runtimeObjects '-Wl,--allow-multiple-definition' -o $asmExe
            if ($LASTEXITCODE -ne 0) { throw 'linking emitted assembly failed' }
            $asmOutput = ((& $asmExe) -join "`n").Trim()
            if ($LASTEXITCODE -ne 0 -or $asmOutput -ne "84`n2.5`nassembly") {
                throw "emitted assembly output mismatch: $asmOutput"
            }
        }
        foreach ($level in 0..3) {
            $levelAsm = Join-Path $outDir "full.O$level.s"
            $levelObj = Join-Path $outDir "full.O$level.obj"
            & $Mira --emit=asm $asmFixture -o $levelAsm "-O$level" | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "O$level assembly emission failed" }
            & gcc -c $levelAsm -o $levelObj
            if ($LASTEXITCODE -ne 0) { throw "GNU assembler rejected O$level assembly" }
        }
        $avxAsm = Join-Path $outDir 'simd-avx2.s'
        $avxObj = Join-Path $outDir 'simd-avx2.obj'
        & $Mira --emit=asm (Join-Path $PSScriptRoot 'simd_add.mira') -o $avxAsm -O3 -mavx2 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'AVX2 assembly emission failed' }
        if ((Get-Content -Raw -LiteralPath $avxAsm) -notmatch '\bymm[0-9]+\b') {
            throw 'AVX2 assembly did not contain a YMM register'
        }
        & gcc -c $avxAsm -o $avxObj
        if ($LASTEXITCODE -ne 0) { throw 'GNU assembler rejected AVX2 assembly' }
        Write-Output 'GNU INTEL ASSEMBLY PASS'
    }
    if ($Group -in @('artifacts', 'all')) {
        $compilerPath = (Resolve-Path -LiteralPath $Mira).Path
        $compilerRoot = Split-Path -Parent $compilerPath
        $isWindowsCompiler = [IO.Path]::GetExtension($compilerPath) -eq '.exe'
        $exeSuffix = if ($isWindowsCompiler) { '.exe' } else { '' }
        $objSuffix = if ($isWindowsCompiler) { '.obj' } else { '.o' }
        Push-Location $outDir
        try {
            & $Mira $fixture -o "custom$exeSuffix" | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "custom$exeSuffix")) {
                throw 'custom executable output failed'
            }
            if (((& ".\custom$exeSuffix") -join "`n").Trim() -ne '42') {
                throw 'custom executable output mismatch'
            }
            & $Mira $fixture | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "final_ir_pipeline$exeSuffix")) {
                throw 'default executable name failed'
            }
            & $Mira -c $fixture | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "final_ir_pipeline$objSuffix")) {
                throw 'default object name failed'
            }
            & $Mira --emit=obj $fixture -o "standalone$objSuffix" | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "standalone$objSuffix")) {
                throw 'custom object output failed'
            }
            if (Test-Path -LiteralPath "standalone$exeSuffix") {
                throw 'object-only mode unexpectedly linked an executable'
            }
            $runtimeObjects = @("rt_core$objSuffix", "rt_print$objSuffix") |
                ForEach-Object { Join-Path $compilerRoot "runtime\$_" }
            & $Mira -l "standalone$objSuffix" $runtimeObjects -o "linked$exeSuffix" | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath "linked$exeSuffix")) {
                throw 'linking emitted object failed'
            }
            if (((& ".\linked$exeSuffix") -join "`n").Trim() -ne '42') {
                throw 'linked object output mismatch'
            }
            & $Mira -S $fixture | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath 'final_ir_pipeline.s')) {
                throw 'default assembly name failed'
            }
            & $Mira --emit=ir $fixture | Out-Host
            if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath 'final_ir_pipeline.ir')) {
                throw 'default IR name failed'
            }
        } finally {
            Pop-Location
        }
        $errorCases = @(
            @{ Args = @('-o'); Text = "option '-o' requires a value" },
            @{ Args = @('-S', '-c', $fixture); Text = 'conflicting emit modes' },
            @{ Args = @($fixture, $asmFixture); Text = 'multiple input files' },
            @{ Args = @('--unknown', $fixture); Text = "unknown option '--unknown'" }
        )
        foreach ($case in $errorCases) {
            $savedPreference = $ErrorActionPreference
            $ErrorActionPreference = 'Continue'
            try { $diagnostic = (& $Mira @($case.Args) 2>&1 | Out-String) }
            finally { $ErrorActionPreference = $savedPreference }
            if ($LASTEXITCODE -eq 0 -or $diagnostic -notmatch [regex]::Escape($case.Text)) {
                throw "diagnostic mismatch for $($case.Args -join ' '): $diagnostic"
            }
        }
        $help = (& $Mira --help | Out-String)
        foreach ($text in @('--emit=asm', '--emit=ir', '--emit=obj', '-o <path>', 'default: -O2')) {
            if ($help -notmatch [regex]::Escape($text)) { throw "help missing: $text" }
        }
        Write-Output 'CLI ARTIFACT OUTPUTS PASS'
    }
    if ($Group -eq 'all') { Write-Output 'CLI EMIT MODES PASS' }
} finally {
    Remove-Item -LiteralPath $outDir -Recurse -Force -ErrorAction SilentlyContinue
}
