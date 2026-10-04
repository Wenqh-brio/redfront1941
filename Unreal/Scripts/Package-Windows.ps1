[CmdletBinding()]
param(
    [string]$EngineRoot = 'E:\UE_5.8',
    [string]$ArchiveDirectory = '',
    [string]$MSBuildPath = ''
)

$ErrorActionPreference = 'Stop'
$UnrealRoot = Split-Path -Parent $PSScriptRoot
$RepositoryRoot = Split-Path -Parent $UnrealRoot
$ProjectFile = Join-Path $UnrealRoot 'RedFront1941.uproject'
$VcxProject = Join-Path $UnrealRoot 'Intermediate\ProjectFiles\RedFront1941.vcxproj'

if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $RepositoryRoot 'dist\WindowsShippingFinal'
}
$ArchiveDirectory = [System.IO.Path]::GetFullPath($ArchiveDirectory, $RepositoryRoot)

if (-not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) {
    throw "Unreal project was not found: $ProjectFile"
}
if (-not (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') -PathType Leaf)) {
    throw "UE 5.8 RunUAT.bat was not found under: $EngineRoot"
}
if (-not (Test-Path -LiteralPath $VcxProject -PathType Leaf)) {
    $GenerateProjectFiles = Join-Path $EngineRoot 'Engine\Build\BatchFiles\GenerateProjectFiles.bat'
    if (-not (Test-Path -LiteralPath $GenerateProjectFiles -PathType Leaf)) {
        throw "Generated MSBuild project is missing and GenerateProjectFiles.bat was not found under $EngineRoot."
    }
    & $GenerateProjectFiles "-project=$ProjectFile" -game -engine
    if ($LASTEXITCODE -ne 0) {
        throw "UE project file generation failed with exit code $LASTEXITCODE."
    }
}
if (-not (Test-Path -LiteralPath $VcxProject -PathType Leaf)) {
    throw "Project file generation completed, but the MSBuild project is still missing: $VcxProject"
}
$ContentDirectory = Join-Path $UnrealRoot 'Content'
$CampaignMaps = @(Get-ChildItem (Join-Path $ContentDirectory 'RedFront\Levels') -Recurse -File -Filter '*.umap' | Sort-Object FullName)
if ($CampaignMaps.Count -ne 56) {
    throw "Expected 56 campaign maps, but found $($CampaignMaps.Count) under Content\RedFront\Levels."
}
$MapsToCook = ($CampaignMaps | ForEach-Object {
    $RelativePath = [System.IO.Path]::GetRelativePath($ContentDirectory, $_.FullName).Replace('\', '/')
    $PackagePath = $RelativePath.Substring(0, $RelativePath.LastIndexOf('.'))
    "/Game/$PackagePath"
}) -join '+'
if (Test-Path -LiteralPath $ArchiveDirectory) {
    throw "Archive output already exists; choose a new -ArchiveDirectory to avoid overwriting it: $ArchiveDirectory"
}

if ([string]::IsNullOrWhiteSpace($MSBuildPath)) {
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $VsWhere -PathType Leaf) {
        $MSBuildPath = & $VsWhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
            Select-Object -First 1
    }
}
if ([string]::IsNullOrWhiteSpace($MSBuildPath) -or -not (Test-Path -LiteralPath $MSBuildPath -PathType Leaf)) {
    throw 'MSBuild.exe was not found. Pass its path with -MSBuildPath.'
}

$ProjectDirectory = "$UnrealRoot\"
& $MSBuildPath $VcxProject /m /p:Configuration=Shipping /p:Platform=x64 "/p:SolutionDir=$ProjectDirectory" /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE."
}

$RunUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$UatArguments = @(
    'BuildCookRun'
    "-project=$ProjectFile"
    '-platform=Win64'
    '-clientconfig=Shipping'
    "-MapsToCook=$MapsToCook"
    '-build'
    '-cook'
    '-cookdir=/Game/RedFront/Levels'
    '-stage'
    '-pak'
    '-iostore'
    '-compressed'
    '-prereqs'
    '-archive'
    "-archivedirectory=$ArchiveDirectory"
    '-unattended'
    '-nop4'
    '-utf8output'
)

& $RunUat @UatArguments
if ($LASTEXITCODE -ne 0) {
    throw "UE BuildCookRun failed with exit code $LASTEXITCODE."
}

$PackagedExecutable = Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File |
    Where-Object { $_.Name -in @('RedFront1941.exe', 'RedFront1941-Win64-Shipping.exe') } |
    Select-Object -First 1
if ($null -eq $PackagedExecutable) {
    throw "BuildCookRun completed but no packaged RedFront1941 executable was found under $ArchiveDirectory."
}

$LevelContracts = @(Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File -Filter 'campaign_*.json')
if ($LevelContracts.Count -lt 5) {
    throw "Packaged contract data is incomplete: found $($LevelContracts.Count) campaign JSON files; expected at least 5."
}

$PrerequisiteInstaller = Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File -Filter 'vc_redist.x64.exe' |
    Select-Object -First 1
if ($null -eq $PrerequisiteInstaller) {
    throw 'The UE packaging prerequisite installer was not staged; the archive is not self-contained.'
}

Write-Host "Shipping executable: $($PackagedExecutable.FullName)"
Write-Host "Campaign contract files: $($LevelContracts.Count)"
Write-Host "VC++ prerequisite installer: $($PrerequisiteInstaller.FullName)"
Write-Host "Archive directory: $ArchiveDirectory"
