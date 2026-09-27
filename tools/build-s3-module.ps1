param(
    [Parameter(Mandatory=$true)][string]$Compiler,
    [Parameter(Mandatory=$true)][string]$ModuleAbiDir,
    [string]$BuildDir = (Join-Path $env:TEMP 'hidpad-s3-module')
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$Compiler = (Resolve-Path -LiteralPath $Compiler).Path
$ModuleAbiDir = (Resolve-Path -LiteralPath $ModuleAbiDir).Path
if (-not (Test-Path -LiteralPath (Join-Path $ModuleAbiDir 'module_abi.h'))) { throw 'module_abi.h missing' }
if ([IO.Path]::GetFileName($Compiler) -ne 'xtensa-esp32s3-elf-gcc.exe') { throw 'Use the native ESP32-S3 compiler' }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$bin = Split-Path $Compiler
$strip = Join-Path $bin 'xtensa-esp-elf-strip.exe'
$readelf = Join-Path $bin 'xtensa-esp-elf-readelf.exe'
$flags = @('-O2','-fPIC','-fvisibility=hidden','-fdata-sections','-ffunction-sections',
    '-fno-jump-tables','-fno-tree-switch-conversion','-fno-tree-loop-distribute-patterns',
    '-fconserve-stack','-fno-builtin-memcpy','-fno-builtin-memmove','-fno-builtin-memset',
    '-fno-builtin-strlen','-Wframe-larger-than=256',"-I$ModuleAbiDir","-I$repo/src/main")
$objects = @()
foreach ($name in @('hidpad_module','hid_report_parser')) {
    $obj = Join-Path $BuildDir "$name.o"
    & $Compiler @flags -c "$repo/src/main/$name.c" -o $obj
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $name" }
    $objects += $obj
}
$libgcc = (& $Compiler -print-libgcc-file-name).Trim()
$output = Join-Path $BuildDir 'hidpad.so'
& $Compiler '-shared' '-fPIC' '-static-libgcc' '-nostdlib' '-nostartfiles' '-Wl,--gc-sections' '-Wl,--strip-debug' '-Wl,-Bsymbolic' '-Wl,--exclude-libs,ALL' '-Wl,-z,notext' '-Wl,--no-check-sections' "-Wl,--version-script=$repo/src/exports.map" "-Wl,-Map,$BuildDir/hidpad_so.map" -o $output @objects $libgcc
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
# Match this repository's ESP32-S3 dynamic module format.
& $strip '--strip-unneeded' '--remove-section=.comment' '--remove-section=.got.loc' '--remove-section=.dynamic' '--remove-section=.xt.lit' '--remove-section=.xt.prop' '--remove-section=.xtensa.info' $output
if ($LASTEXITCODE -ne 0) { throw 'Strip failed' }
$header = & $readelf -h $output
if ($LASTEXITCODE -ne 0 -or -not ($header -match 'Xtensa') -or -not ($header -match 'DYN')) { throw 'Unexpected ELF target/type' }
$symbols = & $readelf --dyn-syms --wide $output
foreach ($export in @('module_query_v1','module_create_v2','module_luaopen_v1','module_destroy_v1')) {
    if (-not ($symbols -match "\b$export\b")) { throw "Missing export: $export" }
}
Copy-Item -LiteralPath $output -Destination (Join-Path $repo 'package/modules/hidpad.so')
Write-Output "S3 module built: $((Get-Item -LiteralPath $output).Length) bytes"
