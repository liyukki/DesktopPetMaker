param(
    [string]$ProjectRoot = "",
    [string]$BuildDir = "",
    [string]$OutputZip = ""
)

$ErrorActionPreference = "Stop"
$env:PYTHONDONTWRITEBYTECODE = "1"
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$scriptDir = Split-Path -Parent $PSCommandPath
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $scriptDir }
$workspaceRoot = Split-Path -Parent $ProjectRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) { $BuildDir = Join-Path $ProjectRoot "build-v6-release" }
if ([string]::IsNullOrWhiteSpace($OutputZip)) { $OutputZip = Join-Path $workspaceRoot "DesktopPet-Runtime-RC.zip" }

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$stage = Join-Path $workspaceRoot "portable-rc-stage"
if (-not $stage.StartsWith($workspaceRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe staging path"
}
Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage | Out-Null

$requiredFiles = @("pro.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Network.dll", "Qt6Widgets.dll")
foreach ($name in $requiredFiles) {
    $source = Join-Path $BuildDir $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing deployed runtime file: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $name)
}
Get-ChildItem -LiteralPath $BuildDir -Filter "*.dll" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $stage $_.Name) -Force
}
foreach ($directory in @("generic", "iconengines", "imageformats", "networkinformation", "platforms", "styles", "tls")) {
    $source = Join-Path $BuildDir $directory
    if (Test-Path -LiteralPath $source -PathType Container) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $stage $directory) -Recurse
    }
}
$translationTarget = Join-Path $stage "translations"
New-Item -ItemType Directory -Path $translationTarget | Out-Null
foreach ($translation in @("qt_zh_CN.qm", "qt_ja.qm", "qt_en.qm")) {
    $source = Join-Path $BuildDir "translations/$translation"
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $translationTarget $translation)
    }
}

Copy-Item -LiteralPath (Join-Path $ProjectRoot "PORTABLE_README.md") -Destination (Join-Path $stage "README.md")
Copy-Item -LiteralPath (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") -Destination $stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot "ASSET_PROVENANCE.md") -Destination $stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot "ASSET_LICENSES.json") -Destination $stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot "LICENSE_COMPLIANCE_APPROVAL.md") -Destination $stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot "licenses") -Destination (Join-Path $stage "licenses") -Recurse

$launcher = "@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" `"%~dp0pro.exe`" --control-center`r`n"
[System.IO.File]::WriteAllText((Join-Path $stage "start_desktop_pet.bat"), $launcher, $utf8NoBom)

$python = (Get-Command python -ErrorAction Stop).Source
$manifestTool = Join-Path $ProjectRoot "tools/create_source_manifest.py"
$verifyTool = Join-Path $ProjectRoot "tools/verify_source_manifest.py"
$zipTool = Join-Path $ProjectRoot "tools/create_standard_zip.py"
$manifest = Join-Path $stage "SOURCE_MANIFEST_SHA256.txt"
$verificationLog = Join-Path $stage "MANIFEST_VERIFICATION_LOG.txt"

& $python $manifestTool --root $stage --output $manifest --all-files --exclude-name "MANIFEST_VERIFICATION_LOG.txt"
if ($LASTEXITCODE -ne 0) { throw "Portable manifest generation failed" }
$verification = (& $python $verifyTool --root $stage --manifest $manifest --all-files `
    --exclude-name "MANIFEST_VERIFICATION_LOG.txt" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "Portable manifest verification failed: $verification" }
[System.IO.File]::WriteAllText(
    $verificationLog,
    "Manifest excludes SOURCE_MANIFEST_SHA256.txt and MANIFEST_VERIFICATION_LOG.txt.`n$verification",
    $utf8NoBom)

& $python $zipTool --root $stage --output $OutputZip
if ($LASTEXITCODE -ne 0) { throw "Portable ZIP generation failed" }

$verifyStage = "$stage-verify"
Remove-Item -LiteralPath $verifyStage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $verifyStage | Out-Null
& $python -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" $OutputZip $verifyStage
if ($LASTEXITCODE -ne 0) { throw "Portable ZIP extraction failed" }
& $python $verifyTool --root $verifyStage `
    --manifest (Join-Path $verifyStage "SOURCE_MANIFEST_SHA256.txt") --all-files `
    --exclude-name "MANIFEST_VERIFICATION_LOG.txt"
if ($LASTEXITCODE -ne 0) { throw "Portable ZIP extraction manifest verification failed" }

$zipHash = (Get-FileHash -LiteralPath $OutputZip -Algorithm SHA256).Hash
$exeHash = (Get-FileHash -LiteralPath (Join-Path $stage "pro.exe") -Algorithm SHA256).Hash
$manifestHash = (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash
$chain = [ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    artifact = [ordered]@{
        path = (Split-Path -Leaf $OutputZip)
        size = (Get-Item -LiteralPath $OutputZip).Length
        sha256 = $zipHash
    }
    executable = [ordered]@{
        path = "pro.exe"
        sha256 = $exeHash
        authenticode = "NOT_SIGNED"
    }
    innerManifest = [ordered]@{
        path = "SOURCE_MANIFEST_SHA256.txt"
        sha256 = $manifestHash
        verification = "PASS"
    }
    samples = [ordered]@{
        included = $false
        reason = "Existing character assets are NOT_CLEARED for public redistribution."
    }
    externalValidation = [ordered]@{
        cleanWindows10Vm = "NOT_TESTED"
        cleanWindows11Vm = "NOT_TESTED"
        multiMonitorHighDpi = "NOT_TESTED"
    }
}
$chainPath = Join-Path (Split-Path -Parent $OutputZip) "FINAL_ARTIFACT_CHAIN.json"
[System.IO.File]::WriteAllText(
    $chainPath,
    ($chain | ConvertTo-Json -Depth 8),
    $utf8NoBom)

Remove-Item -LiteralPath $verifyStage -Recurse -Force
Get-Item -LiteralPath $OutputZip
Get-FileHash -LiteralPath $OutputZip -Algorithm SHA256
Get-Item -LiteralPath $chainPath
