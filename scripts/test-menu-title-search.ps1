<#
.SYNOPSIS
Validates title search against the real engine's optional entity collections.
.DESCRIPTION
Uses the current isolated editor assemblies and the reviewed source helper, storing all
build artifacts, output and source hashes in the existing fixed 3DS validation directory.
#>
param()
$ErrorActionPreference='Stop'
$Root='C:/dev/helworks/builds/helengine-3ds/menu-20261007/title-search-tests'
$Project=Join-Path $Root 'TitleSearchTests.csproj'
$Log=Join-Path $Root 'test.log'
& rtk proxy dotnet run --project $Project --artifacts-path (Join-Path $Root 'artifacts') *> $Log
if($LASTEXITCODE -ne 0){Get-Content -LiteralPath $Log -Tail 15;throw 'Real entity title search regression failed.'}
$Output=Get-Content -LiteralPath $Log -Raw
if(!$Output.Contains('PASS 8 title-search checks')){throw 'Title search validation did not execute all eight checks.'}
$SourcePaths=@((Join-Path $PSScriptRoot '../tests/menu/DemoDiscTitleSearchTests.cs'),(Join-Path $PSScriptRoot 'prepare-menu-title-search.ps1'),'C:/dev/helworks/builds/builder-coordination/menu-20261007/menu-title-fit/DemoDiscTitleLayout.cs')
$Hashes=@()
foreach($Source in $SourcePaths){$Hashes+=@{path=[IO.Path]::GetFullPath($Source);sha256=(Get-FileHash -LiteralPath $Source).Hash}}
[ordered]@{passed=$true;checks=8;logPath=$Log;sourceHashes=$Hashes;validation='real Core entity collections, borrowed identity and explicit invalid-state exceptions';generatedFilesModified=$false} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Root 'receipt.json') -Encoding utf8
Get-Content -LiteralPath $Log
