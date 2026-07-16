$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
git -C $repoRoot config --local core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw "Unable to configure the repository-local hooks path."
}

Write-Host "Repository-local Git hooks enabled: .githooks"
