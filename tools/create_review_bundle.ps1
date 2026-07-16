param(
    [string]$Root = "",
    [string]$OutputZip = ""
)

$ErrorActionPreference = "Stop"
$env:PYTHONDONTWRITEBYTECODE = "1"

$scriptDir = Split-Path -Parent $PSCommandPath
$projectRootDefault = Split-Path -Parent $scriptDir
$rootDefault = Split-Path -Parent $projectRootDefault
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = $rootDefault
}
if ([string]::IsNullOrWhiteSpace($OutputZip)) {
    $OutputZip = Join-Path $Root "desktop_pet_ai_review_portable.zip"
}

$projectRoot = Join-Path $Root "pro"
$tempRoot = Join-Path $env:TEMP ("desktop_pet_review_extract_" + [guid]::NewGuid().ToString("N"))
$diffPath = Join-Path $projectRoot "CURRENT_PATCH_FOR_REVIEW.diff"
$encodingCheck = Join-Path $projectRoot "tools/check_text_encoding.py"
$sourceCheck = Join-Path $projectRoot "tools/check_cmake_source_completeness.py"
$releasePetCheck = Join-Path $projectRoot "tools/validate_release_pets.py"
$claimsCheck = Join-Path $projectRoot "tools/verify_final_claims.py"
$manifestVerifyTool = Join-Path $projectRoot "tools/verify_source_manifest.py"
$candidateZip = Join-Path (Split-Path -Parent $OutputZip) `
    ("." + (Split-Path -Leaf $OutputZip) + "." + [guid]::NewGuid().ToString("N") + ".tmp.zip")
trap {
    Remove-Item -LiteralPath $candidateZip -Force -ErrorAction SilentlyContinue
    throw $_
}
if (Test-Path -LiteralPath $encodingCheck -PathType Leaf) {
    Push-Location $projectRoot
    try {
        & python $encodingCheck
        if ($LASTEXITCODE -ne 0) {
            throw "Text encoding check failed; review bundle generation aborted."
        }
    }
    finally {
        Pop-Location
    }
}
if (Test-Path -LiteralPath $sourceCheck -PathType Leaf) {
    Push-Location $projectRoot
    try {
        & python $sourceCheck $projectRoot
        if ($LASTEXITCODE -ne 0) {
            throw "CMake source completeness check failed; review bundle generation aborted."
        }
    }
    finally {
        Pop-Location
    }
}
if (Test-Path -LiteralPath $releasePetCheck -PathType Leaf) {
    Push-Location $projectRoot
    try {
        & python $releasePetCheck
        if ($LASTEXITCODE -ne 0) {
            throw "Release pet validation failed; review bundle generation aborted."
        }
    }
    finally {
        Pop-Location
    }
}
if (Test-Path -LiteralPath $claimsCheck -PathType Leaf) {
    Push-Location $projectRoot
    try {
        & python $claimsCheck --pre-report --root $projectRoot
        if ($LASTEXITCODE -ne 0) {
            throw "Final-claims verification failed; review bundle generation aborted."
        }
    }
    finally {
        Pop-Location
    }
}
if (Test-Path -LiteralPath $manifestVerifyTool -PathType Leaf) {
    Push-Location $projectRoot
    try {
        & python $manifestVerifyTool --root $projectRoot `
            --manifest (Join-Path $projectRoot "SOURCE_MANIFEST_SHA256.txt")
        if ($LASTEXITCODE -ne 0) {
            throw "Source manifest is stale; run update_source_manifest explicitly before review bundle generation."
        }
    }
    finally {
        Pop-Location
    }
}

Push-Location $projectRoot
try {
    $diffInputs = @(
        ".gitignore","CMakeLists.txt","desktop_pet.rc","resources.qrc",
        "SOURCE_MANIFEST_SHA256.txt","ASSET_LICENSES.json",".githooks/pre-commit"
    )
    foreach ($sourceDirectory in @("src", "tests", "tools")) {
        $sourcePath = Join-Path $projectRoot $sourceDirectory
        if (Test-Path -LiteralPath $sourcePath -PathType Container) {
            $diffInputs += Get-ChildItem -LiteralPath $sourcePath -Recurse -File | ForEach-Object {
                $_.FullName.Substring($projectRoot.Length + 1)
            }
        }
    }
    $diffInputs += Get-ChildItem -LiteralPath $projectRoot -Filter "*.md" -File | ForEach-Object { $_.Name }
    $untracked = & git ls-files --others --exclude-standard -- @diffInputs
    if ($untracked) {
        & git add -N -- $untracked
        if ($LASTEXITCODE -ne 0) {
            throw "git add -N failed while preparing review diff."
        }
    }
    try {
        $diffText = & git diff -- @diffInputs
    }
    finally {
        if ($untracked) {
            & git reset -- $untracked | Out-Null
        }
    }
    $cachedAfterDiff = & git diff --cached --name-only
    if ($cachedAfterDiff) {
        throw "Review diff generation left staged changes: $($cachedAfterDiff -join ', ')"
    }
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($diffPath, ($diffText -join "`n") + "`n", $utf8NoBom)
}
finally {
    Pop-Location
}

$diffBytes = [System.IO.File]::ReadAllBytes($diffPath)
if ($diffBytes.Length -ge 2 -and (($diffBytes[0] -eq 0xFF -and $diffBytes[1] -eq 0xFE) -or ($diffBytes[0] -eq 0xFE -and $diffBytes[1] -eq 0xFF))) {
    throw "Diff encoding check failed: UTF-16 BOM detected"
}
if ($diffBytes -contains 0) {
    throw "Diff encoding check failed: NUL byte detected"
}
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$decodedDiff = $strictUtf8.GetString($diffBytes)
if (-not $decodedDiff.StartsWith("diff --git")) {
    throw "Diff encoding check failed: first line does not start with diff --git"
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zip = [System.IO.Compression.ZipFile]::Open($candidateZip, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    function Add-FileToZip([string]$Source, [string]$EntryName) {
        if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
            return
        }
        $portableName = $EntryName -replace '\\', '/'
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($script:zip, $Source, $portableName, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }

    function Add-DirectoryToZip([string]$SourceDir, [string]$EntryPrefix) {
        if (-not (Test-Path -LiteralPath $SourceDir -PathType Container)) {
            return
        }
        $base = (Resolve-Path -LiteralPath $SourceDir).Path.TrimEnd('\') + '\'
        Get-ChildItem -LiteralPath $SourceDir -Recurse -File | Where-Object {
            $_.FullName -notmatch '[\\/]__pycache__[\\/]'
        } | ForEach-Object {
            $relative = $_.FullName.Substring($base.Length)
            Add-FileToZip $_.FullName (($EntryPrefix.TrimEnd('/')) + "/" + ($relative -replace '\\', '/'))
        }
    }

    $proFiles = @(
        ".gitignore","CMakeLists.txt","desktop_pet.rc","resources.qrc","SOURCE_MANIFEST_SHA256.txt",
        ".githooks/pre-commit",
        "V5_RELEASE_CONFIGURE_LOG.txt","V5_RELEASE_BUILD_LOG.txt","V5_RELEASE_FINAL_BUILD_LOG.txt",
        "V5_DEBUG_CONFIGURE_LOG.txt","V5_DEBUG_BUILD_LOG.txt","V5_DEBUG_FINAL_BUILD_LOG.txt",
        "V5_CLEAN_RELEASE_SMOKE_LOG.txt","V5_CLEAN_DEBUG_SMOKE_LOG.txt","V5_TEXT_ENCODING_LOG.txt",
        "V5_CMAKE_SOURCE_CHECK_LOG.txt","V5_RELEASE_PETS_LOG.txt","V5_CLAIMS_CHECK_LOG.txt",
        "V6_RELEASE_CONFIGURE_LOG.txt","V6_RELEASE_BUILD_LOG.txt","V6_RELEASE_CTEST_LOG.txt","V6_RELEASE_CTEST_RESULTS.xml",
        "V6_DEBUG_CONFIGURE_LOG.txt","V6_DEBUG_BUILD_LOG.txt","V6_DEBUG_CTEST_LOG.txt","V6_DEBUG_CTEST_RESULTS.xml",
        "V6_FINAL_DEBUG_CTEST_RESULTS.xml","V6_FINAL_RELEASE_CTEST_RESULTS.xml",
        "V7_DEBUG_CONFIGURE_LOG.txt","V7_DEBUG_BUILD_LOG.txt","V7_DEBUG_CTEST_LOG.txt",
        "V7_RELEASE_CONFIGURE_LOG.txt","V7_RELEASE_BUILD_LOG.txt","V7_RELEASE_CTEST_LOG.txt",
        "V7_FINAL_DEBUG_CTEST_RESULTS.xml","V7_FINAL_RELEASE_CTEST_RESULTS.xml",
        "CURRENT_PATCH_FOR_REVIEW.diff"
    )
    foreach ($file in $proFiles) {
        Add-FileToZip (Join-Path $projectRoot $file) ("pro/" + $file)
    }

    Get-ChildItem -LiteralPath $projectRoot -Filter "*.md" -File | ForEach-Object {
        Add-FileToZip $_.FullName ("pro/" + $_.Name)
    }
    Add-DirectoryToZip (Join-Path $Root "pet_animated") "pet_animated"
    Add-DirectoryToZip (Join-Path $Root "pet_animated_1") "pet_animated_1"
    Add-DirectoryToZip (Join-Path $Root "legacy") "legacy"
    Add-DirectoryToZip (Join-Path $projectRoot "licenses") "pro/licenses"
    Add-DirectoryToZip (Join-Path $projectRoot "resources") "pro/resources"
    Add-DirectoryToZip (Join-Path $projectRoot "tools") "pro/tools"
    Add-DirectoryToZip (Join-Path $projectRoot "tests") "pro/tests"
    Add-DirectoryToZip (Join-Path $projectRoot "src") "pro/src"
    Add-FileToZip "D:\DESKTOP_PET_PLATFORM_STRICT_SCORE_PORTABLE3_20260714.md" "review-context/DESKTOP_PET_PLATFORM_STRICT_SCORE_PORTABLE3_20260714.md"
    Add-FileToZip "D:\CODEX_STRICT_NEXT_REMEDIATION_PORTABLE3_20260714.md" "review-context/CODEX_STRICT_NEXT_REMEDIATION_PORTABLE3_20260714.md"
    Add-FileToZip "D:\CODEX_STRICT_REAUDIT_REMEDIATION_20260715_V4.md" "review-context/CODEX_STRICT_REAUDIT_REMEDIATION_20260715_V4.md"
    Add-FileToZip "D:\CODEX_STRICT_REAUDIT_REMEDIATION_20260715_V5.md" "review-context/CODEX_STRICT_REAUDIT_REMEDIATION_20260715_V5.md"
    Add-FileToZip "D:\CODEX_STRICT_REAUDIT_REMEDIATION_V6_FINAL.md" "review-context/CODEX_STRICT_REAUDIT_REMEDIATION_V6_FINAL.md"
    Add-FileToZip "D:\CODEX_STRICT_REAUDIT_REMEDIATION_PORTABLE3_20260714_V2.md" "review-context/CODEX_STRICT_REAUDIT_REMEDIATION_PORTABLE3_20260714_V2.md"
    Add-FileToZip "D:\CODEX_PORTABLE_PACKAGE_COMBINED_FINAL_REMEDIATION_20260716.md" "review-context/CODEX_PORTABLE_PACKAGE_COMBINED_FINAL_REMEDIATION_20260716.md"
    Add-FileToZip "D:\CODEX_RC_FINAL_RELEASE_REMEDIATION_20260716.md" "review-context/CODEX_RC_FINAL_RELEASE_REMEDIATION_20260716.md"
}
finally {
    $zip.Dispose()
}

$badEntries = [System.IO.Compression.ZipFile]::OpenRead($candidateZip)
try {
    $bad = $badEntries.Entries | Where-Object { $_.FullName.Contains('\') }
    if ($bad) {
        throw "ZIP portability check failed: backslash entry found: $($bad[0].FullName)"
    }
}
finally {
    $badEntries.Dispose()
}

Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $tempRoot | Out-Null
[System.IO.Compression.ZipFile]::ExtractToDirectory($candidateZip, $tempRoot)

$required = @(
    "pro/CMakeLists.txt",
    "pro/src/runtime/runtimepetwindow.cpp",
    "pro/src/runtime/runtimepetmanager.cpp",
    "pro/src/ai/core/petairequestcoordinator.cpp",
    "pro/src/ai/chat/aiconversationroomrepository.cpp",
    "pro/src/ai/chat/aiconversationroom.cpp",
    "pro/src/runtime/petruntimeinstance.cpp",
    "pro/src/project/petprojectregistry.cpp",
    "pro/src/app/petcontrolcenterwindow.cpp",
    "pro/tests/platform_smoke_tests.cpp",
    "pro/src/runtime/renderbackend.cpp",
    "pro/tests/runtime_action_tests.cpp",
    "pro/tests/gui_widget_tests.cpp",
    "pro/PLATFORM_IMPLEMENTATION_REPORT.md",
    "pro/THIRD_PARTY_NOTICES.md",
    "pro/src/runtime/petspeechbubblewindow.cpp",
    "pro/src/ai/core/petconversationcontext.h",
    "pro/src/ai/core/chathistoryentry.h",
    "pro/tools/check_text_encoding.py",
    "pro/tools/check_cmake_source_completeness.py",
    "pro/tools/verify_final_claims.py",
    "pro/tools/create_review_bundle.ps1",
    "pro/tools/create_source_manifest.py",
    "pro/tools/verify_source_manifest.py",
    "pet_animated/pet.json",
    "pet_animated_1/pet.json"
)
foreach ($item in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $tempRoot $item) -PathType Leaf)) {
        throw "ZIP extract check failed: missing $item"
    }
}

$extractedProjectRoot = Join-Path $tempRoot "pro"
Get-ChildItem -LiteralPath $tempRoot -Directory -Filter "__pycache__" -Recurse -ErrorAction SilentlyContinue |
    Remove-Item -Recurse -Force
$extractedSourceCheck = Join-Path $extractedProjectRoot "tools/check_cmake_source_completeness.py"
if (Test-Path -LiteralPath $extractedSourceCheck -PathType Leaf) {
    Push-Location $extractedProjectRoot
    try {
        & python $extractedSourceCheck $extractedProjectRoot
        if ($LASTEXITCODE -ne 0) {
            throw "ZIP extract source completeness check failed."
        }
    }
    finally {
        Pop-Location
    }
}
$extractedClaimsCheck = Join-Path $extractedProjectRoot "tools/verify_final_claims.py"
if (Test-Path -LiteralPath $extractedClaimsCheck -PathType Leaf) {
    Push-Location $extractedProjectRoot
    try {
        & python $extractedClaimsCheck --pre-report --root $extractedProjectRoot
        if ($LASTEXITCODE -ne 0) {
            throw "ZIP extract final-claims verification failed."
        }
    }
    finally {
        Pop-Location
    }
}

# The root manifest describes the final portable bundle, not the source workspace.
Remove-Item -LiteralPath (Join-Path $extractedProjectRoot "SOURCE_MANIFEST_SHA256.txt") -Force -ErrorAction SilentlyContinue
$bundleManifest = Join-Path $tempRoot "SOURCE_MANIFEST_SHA256.txt"
$bundleManifestLog = Join-Path $tempRoot "MANIFEST_VERIFICATION_LOG.txt"
$createManifest = Join-Path $extractedProjectRoot "tools/create_source_manifest.py"
$verifyManifest = Join-Path $extractedProjectRoot "tools/verify_source_manifest.py"
& python $createManifest --root $tempRoot --output $bundleManifest --all-files `
    --exclude-name "MANIFEST_VERIFICATION_LOG.txt"
if ($LASTEXITCODE -ne 0) {
    throw "Bundle manifest generation failed."
}
$verificationOutput = (& python $verifyManifest --root $tempRoot --manifest $bundleManifest --all-files `
    --exclude-name "MANIFEST_VERIFICATION_LOG.txt" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Bundle manifest verification failed: $verificationOutput"
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    $bundleManifestLog,
    "Manifest excludes SOURCE_MANIFEST_SHA256.txt and MANIFEST_VERIFICATION_LOG.txt.`n$verificationOutput",
    $utf8NoBom)

$standardZipTool = Join-Path $extractedProjectRoot "tools/create_standard_zip.py"
& python $standardZipTool --root $tempRoot --output $candidateZip
if ($LASTEXITCODE -ne 0) {
    throw "Standard ZIP rebuild failed."
}

$verifyExtract = "$tempRoot-verify"
Remove-Item -LiteralPath $verifyExtract -Recurse -Force -ErrorAction SilentlyContinue
[System.IO.Compression.ZipFile]::ExtractToDirectory($candidateZip, $verifyExtract)
$zipVerification = (& python (Join-Path $verifyExtract "pro/tools/verify_source_manifest.py") `
    --root $verifyExtract `
    --manifest (Join-Path $verifyExtract "SOURCE_MANIFEST_SHA256.txt") `
    --all-files `
    --exclude-name "MANIFEST_VERIFICATION_LOG.txt" 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Rebuilt ZIP manifest verification failed: $zipVerification"
}
Remove-Item -LiteralPath $verifyExtract -Recurse -Force -ErrorAction SilentlyContinue

Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
& python -c "import os,sys; os.replace(sys.argv[1], sys.argv[2])" $candidateZip $OutputZip
if ($LASTEXITCODE -ne 0) {
    throw "Atomic review bundle replacement failed."
}
Get-Item -LiteralPath $OutputZip
