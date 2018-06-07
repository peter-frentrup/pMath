
[CmdletBinding()]
param(
	[string]
	$TargetExe,
	
	[string]
	$DllSearchPath
)

If( -not (Test-Path $TargetExe -PathType Leaf)) {
	throw "Executable $TargetExe not found."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$moduleCacheDirectory = (Join-Path $scriptDir "dependency-cache")
$moduleCacheFile = (Join-Path $moduleCacheDirectory ((Get-Item -Path $TargetExe).Directory.Name + "_" + (Get-Item -Path $TargetExe).Name + "_modules.txt"))

Function Get-SourceDlls([string] $TargetExe, [string] $DllSearchPath) {
	$targetDirectory = (Get-Item -Path $TargetExe).Directory.FullName
	$targetName = (Get-Item -Path $TargetExe).Name
	
	$tmpDirectory = "$targetDirectory\tmp"
	$tmpExe = "$tmpDirectory\$targetName"
	If( Test-Path $tmpDirectory ) {
		Remove-Item -Path $tmpDirectory -Force -Recurse
	}
	New-Item -Path $tmpDirectory -ItemType Directory | Out-Null
	Copy-Item -Path $TargetExe -Destination $tmpExe

	$allowedPaths = New-Object System.Collections.ArrayList
	ForEach( $dir in $DllSearchPath.Split(";") ) {
		If( Test-Path $dir ) {
			$normalizedDir = (Get-Item -Path "$dir\.").FullName.ToLower()
			Write-Verbose "Consider $normalizedDir"
			# Remove trailing slash if possible by going via $dir\.
			$allowedPaths.Add( $normalizedDir ) > $null
		}
	}

	$modules = . "$scriptDir\depends.ps1" -Library $tmpExe -SearchPath $DllSearchPath
	
	ForEach( $module in $modules ) {
		If( ($module.Status -eq '') -or ($module.Status -eq '6') ) {
			$file = Get-Item -Path $module.Module -Force
			$dir = $file.Directory.FullName
			If( $allowedPaths.Contains($dir.ToLower())) {
				Write-Output $file.Directory.GetFiles($file.Name)[0].FullName
			}
			Else {
				Write-Verbose "Skip $($file.Name) from $dir"
			}
		}
		ElseIf($module.Module -like 'api-ms-*') {
			Write-Verbose "$($module.Status): Ignore Windows module $($module.Module)"
		}
		ElseIf($module.Status -eq '?') {
			Write-Error "Module not found: $($module.Module)"
		}
		Else {
			Write-Verbose "$($module.Status): Ignore module $($module.Module)"
		}
	}
	
	Remove-Item -Path $tmpDirectory -Force -Recurse
}

If( Test-Path $moduleCacheFile ) {
	Write-Verbose "Using dependency cache file $moduleCacheFile"
	$sourceDlls = Get-Content $moduleCacheFile
}
Else {
	Write-Verbose "Cache file $moduleCacheFile not found. Rebuilding..."
	If( -not (Test-Path $moduleCacheDirectory)) {
		New-Item -Type Directory $moduleCacheDirectory > $null
	}
	
	$sourceDlls = Get-SourceDlls -TargetExe $TargetExe -DllSearchPath $DllSearchPath
	$sourceDlls | Out-File $moduleCacheFile
}

#Write-Verbose "Dependencies:"
#$sourceDlls | Write-Verbose

$targetDirectory = (Get-Item -Path $TargetExe).Directory.FullName

ForEach( $dll in $sourceDlls ) {
	Write-Verbose "Copying $dll ..."
	Copy-Item -Path $dll -Destination $targetDirectory
}
