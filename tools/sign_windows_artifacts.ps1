param(
    [Parameter(Mandatory = $true)]
    [string[]]$Files,
    [string]$CertificateThumbprint = "",
    [string]$PfxPath = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if (-not $signtool) {
    throw "signtool.exe was not found. Install the Windows SDK signing tools."
}
if ([string]::IsNullOrWhiteSpace($CertificateThumbprint) -and
    [string]::IsNullOrWhiteSpace($PfxPath)) {
    throw "Provide CertificateThumbprint or PfxPath. Secrets must not be committed."
}

foreach ($file in $Files) {
    $resolved = (Resolve-Path -LiteralPath $file).Path
    $arguments = @("sign", "/fd", "SHA256", "/tr", $TimestampUrl, "/td", "SHA256")
    if (-not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        $arguments += @("/sha1", $CertificateThumbprint)
    } else {
        $arguments += @("/f", (Resolve-Path -LiteralPath $PfxPath).Path)
    }
    $arguments += $resolved
    & $signtool.Source @arguments
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $resolved" }
    & $signtool.Source verify /pa /all $resolved
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $resolved" }
}
