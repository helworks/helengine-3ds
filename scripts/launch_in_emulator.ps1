param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactPath,

    [Parameter()]
    [string]$EmulatorPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ResolvedArtifactPath = [System.IO.Path]::GetFullPath($ArtifactPath)
if (-not (Test-Path -LiteralPath $ResolvedArtifactPath -PathType Leaf)) {
    throw "Artifact was not found at '$ResolvedArtifactPath'."
}

if ([System.IO.Path]::GetExtension($ResolvedArtifactPath) -ine ".3dsx") {
    throw "Expected a .3dsx artifact but got '$ResolvedArtifactPath'."
}

$DefaultEmulatorCandidates = @(
    "C:\dev\helworks\emus\Lime3DS\lime3ds-qt.exe",
    "C:\dev\helworks\emus\Azahar\azahar.exe"
)

if ([string]::IsNullOrWhiteSpace($EmulatorPath)) {
    $ResolvedEmulatorPath = $DefaultEmulatorCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if ($null -eq $ResolvedEmulatorPath) {
        throw "No Nintendo 3DS emulator was found. Checked: $($DefaultEmulatorCandidates -join ', ')"
    }
} else {
    $ResolvedEmulatorPath = [System.IO.Path]::GetFullPath($EmulatorPath)
    if (-not (Test-Path -LiteralPath $ResolvedEmulatorPath -PathType Leaf)) {
        throw "Emulator was not found at '$ResolvedEmulatorPath'."
    }
}

$process = Start-Process -FilePath $ResolvedEmulatorPath -ArgumentList @($ResolvedArtifactPath) -WorkingDirectory (Split-Path -Path $ResolvedEmulatorPath -Parent) -PassThru
Write-Output ("ARTIFACT=" + $ResolvedArtifactPath)
Write-Output ("EMULATOR=" + $ResolvedEmulatorPath)
Write-Output ("PROCESS_ID=" + $process.Id)
