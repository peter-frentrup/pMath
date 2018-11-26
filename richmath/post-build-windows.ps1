
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

$targetDirectory = (Get-Item -Path $TargetExe).Directory.FullName

Copy-Item -Path "$scriptDir\dependencies\default\*" -Destination $targetDirectory -Force

. "$scriptDir\..\util\collect-dlls.ps1" $TargetExe $DllSearchPath
