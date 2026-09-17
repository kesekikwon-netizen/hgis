# Inspect by default; sign the final EXE only with an explicitly selected certificate.
[CmdletBinding(DefaultParameterSetName = 'Inspect')]
param(
  [string]$ExePath = '',
  [Parameter(ParameterSetName = 'Inspect')]
  [switch]$CheckOnly,
  [Parameter(Mandatory, ParameterSetName = 'Sign')]
  [switch]$Sign,
  [Parameter(Mandatory, ParameterSetName = 'Sign')]
  [ValidatePattern('^[A-Fa-f0-9]{40}$')]
  [string]$CertificateThumbprint,
  [Parameter(ParameterSetName = 'Sign')]
  [ValidateSet('CurrentUser', 'LocalMachine')]
  [string]$CertificateStore = 'CurrentUser',
  [Parameter(Mandatory, ParameterSetName = 'Sign')]
  [ValidatePattern('^https?://')]
  [string]$TimestampUrl,
  [Parameter(Mandatory, ParameterSetName = 'Sign')]
  [string]$SignToolPath
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$target = if ($ExePath) { $ExePath } else { Join-Path $root 'build/Release/ka-hgis.exe' }
if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "EXE missing: $target" }
$target = (Get-Item -LiteralPath $target).FullName
if ([IO.Path]::GetExtension($target) -ine '.exe') { throw 'Select a release .exe file.' }

if ($Sign) {
  # Certificate metadata only; no PFX export, passwords or private key logging.
  $certPath = "Cert:\${CertificateStore}\My\$CertificateThumbprint"
  if (-not (Test-Path -LiteralPath $certPath)) {
    throw "Code-signing certificate missing: $certPath. Obtain a trusted publisher certificate first."
  }
  $cert = Get-Item -LiteralPath $certPath
  $now = Get-Date
  if (-not $cert.HasPrivateKey -or $cert.NotBefore -gt $now -or $cert.NotAfter -le $now) {
    throw 'The signing certificate must be current and have an accessible private key.'
  }
  if ('1.3.6.1.5.5.7.3.3' -notin @($cert.EnhancedKeyUsageList | ForEach-Object { $_.ObjectId })) {
    throw 'The certificate does not have the Code Signing enhanced key usage.'
  }
  if ($cert.PublicKey.Oid.Value -ne '1.2.840.113549.1.1.1') {
    throw 'Smart App Control requires an RSA code-signing certificate.'
  }
  if (-not (Test-Path -LiteralPath $SignToolPath -PathType Leaf)) { throw "SignTool missing: $SignToolPath" }
  $signArgs = @('sign', '/sha1', $CertificateThumbprint, '/s', 'My', '/fd', 'SHA256',
    '/tr', $TimestampUrl, '/td', 'SHA256')
  if ($CertificateStore -eq 'LocalMachine') { $signArgs += '/sm' }
  & $SignToolPath @signArgs $target
  if ($LASTEXITCODE -ne 0) { throw "Signing/timestamp failed ($LASTEXITCODE); do not distribute this EXE." }
  & $SignToolPath verify /pa /all /tw $target
  if ($LASTEXITCODE -ne 0) { throw "Signature verification failed ($LASTEXITCODE); do not distribute this EXE." }
}

$signature = Get-AuthenticodeSignature -LiteralPath $target
if ($Sign -and ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate)) {
  throw 'A valid timestamped signature was not found; do not distribute this EXE.'
}
[pscustomobject]@{
  Path = $target
  SignatureStatus = [string]$signature.Status
  Publisher = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { '' }
  Timestamped = [bool]$signature.TimeStamperCertificate
  SHA256 = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
}
if ($signature.Status -ne 'Valid') {
  Write-Warning 'This EXE has no valid trusted signature. Windows may block it. See docs/portable-desktop.md.'
}
