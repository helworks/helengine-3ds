<#
.SYNOPSIS
Preserves title binding across legitimate entities whose optional collections are uninitialized.
.DESCRIPTION
Applies only the reviewed source-level Find change to the isolated 3DS menu stage. Both the
approved original and corrected source hashes are accepted, making repeated exports idempotent.
Generated files, layout fitting, and the original DemoDisc project are never modified.
#>
param([Parameter(Mandatory=$true)][string]$StagedProjectRoot)
$ErrorActionPreference='Stop'
$Stage=[IO.Path]::GetFullPath($StagedProjectRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
$ExpectedStage=[IO.Path]::GetFullPath('C:/dev/helworks/builds/helengine-3ds/menu-20261007/staged-project')
if($Stage -ne $ExpectedStage){throw 'Title search preparation requires the approved fixed 3DS stage.'}
$Source=Join-Path $Stage 'assets/codebase/menu/DemoDiscTitleLayout.cs'
foreach($Path in @($Stage,(Join-Path $Stage 'assets'),(Join-Path $Stage 'assets/codebase'),(Join-Path $Stage 'assets/codebase/menu'),$Source)) {
    if((Get-Item -LiteralPath $Path).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Title source staging must not traverse a reparse point.'}
}
$Before=(Get-FileHash -LiteralPath $Source).Hash
$Original='8C41C5FB8526127EE4B600DBE3C8264505BD7D8312CC189F95838DE05C676925'
$Corrected='F41F2F2E9EBDE3A50ABFF878226DD41331C15D7E2C1021F1F54D56B0C0E73198'
if($Before -notin @($Original,$Corrected)){throw 'The title helper differs from its reviewed original and corrected source states.'}
if($Before -eq $Original) {
    $Text=[IO.File]::ReadAllText($Source)
    $Text=$Text.Replace('Finds a uniquely named heading once during menu binding without taking ownership of its component.','Finds a uniquely named heading once during menu binding, including valid leaves with uninitialized child collections, without taking ownership of its component.')
    $Newline=if($Text.Contains("`r`n")){"`r`n"}else{"`n"}
    $Text=$Text.Replace('public static TextComponent Find(Entity entity, string content) {',('public static TextComponent Find(Entity entity, string content) {'+$Newline+'            if (entity == null) {'+$Newline+'                throw new ArgumentNullException(nameof(entity));'+$Newline+'            }'))
    $Text=$Text.Replace('index < entity.Components.Count','entity.Components != null && index < entity.Components.Count')
    $Text=$Text.Replace('index < entity.Children.Count','entity.Children != null && index < entity.Children.Count')
    $Bytes=[Text.UTF8Encoding]::new($false).GetBytes($Text)
    $Proposed=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes))
    if($Proposed -ne $Corrected){throw 'The source transformation did not reproduce the reviewed corrected helper.'}
    if((Get-FileHash -LiteralPath $Source).Hash -ne $Before){throw 'The title helper changed during preparation.'}
    [IO.File]::WriteAllBytes($Source,$Bytes)
}
[ordered]@{policy='find-through-optional-entity-collections';sourcePath=$Source;beforeSha256=$Before;afterSha256=$Corrected;idempotent=($Before -eq $Corrected);generatedFilesModified=$false;originalProjectModified=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Stage 'title-search-staging-receipt.json') -Encoding utf8
Write-Output 'Reviewed 3DS title search source is ready.'
