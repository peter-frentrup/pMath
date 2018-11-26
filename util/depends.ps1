<#
.SYNOPSIS
Get the dependend modules of an EXE/DLL.

.DESCRIPTION
This is a wrapper around Depency Walker from http://www.dependencywalker.com/

.PARAMETER Library
The EXE/DLL to inspect

.PARAMETER SearchPath
An optional search path for dependent DLLs as a semicolon separated list of directories.

.PARAMETER DepencyWalker
Location of the depends.exe. If not specified, the script tries to find one in %ProgramFiles%

.EXAMPLE
depends.ps1 `
	-Library D:\gtk-build\releases\Win32\pango\bin\pangocairo-1.0.dll `
	-SearchPath "D:\gtk-build\releases\Win32\cairo\bin;D:\gtk-build\gtk\Win32\bin" `
| Where-Object { $_.Status -eq '' } | Format-Table -AutoSize

#>

[CmdletBinding()]
Param(
	[string]
	[Parameter(mandatory=$true)]
	$Library,
	
	[string]
	$SearchPath = '',
	
	[string]
	$DependencyWalker = ''
)

$libraryItem = Get-Item $Library
$libraryDirectory = $libraryItem.Directory.FullName

If( $DependencyWalker -eq '') {
	$paths = @( $env:ProgramFiles )
	If( ($env:ProgramW6432 -ne $null) -and ($env:ProgramW6432 -ne $env:ProgramFiles)) {
		$paths+= $env:ProgramW6432
	}
	If( (${env:ProgramFiles(x86)} -ne $null) -and (${env:ProgramFiles(x86)} -ne $env:ProgramFiles ) ) {
		$paths+= ${env:ProgramFiles(x86)}
	}
	Write-Verbose "Search for depends.exe in $paths"
	$dirs = $paths
	$DependencyWalker = $null
	$maxDepth = 3
	While(($maxDepth -gt 0) -and ("$DependencyWalker" -eq "")) {
		$maxDepth = $maxDepth - 1
		$dirs = ($dirs | ForEach-Object { Get-ChildItem -Erroraction SilentlyContinue -Path $_ -Directory }).FullName
		$DependencyWalker = ($dirs | ForEach-Object { Get-ChildItem -Erroraction SilentlyContinue -Path $_ -File -Filter "depends.exe" } | Select-Object -First 1).FullName
	}
	If( "$DependencyWalker" -eq "" ) {
		throw "Cannot find depends.exe"
	}
}

Write-Verbose "DepencyWalker is $DependencyWalker"

$oldPath = $env:Path
$newPath = $env:Path
If( $SearchPath -ne '') {
	$newPath = "$SearchPath;$newPath"
}
$newPath = "$libraryDirectory;$newPath"

$output = [System.IO.Path]::GetTempFileName()

Try {
	$env:Path = $newPath
	
	Start-Process -Wait $DependencyWalker -ArgumentList @( "/c", "/f:1", "/oc", $output, $($libraryItem.FullName) )
}
Finally {
	$env:Path = $oldPath
}

$modules = Import-Csv $output

Remove-Item $output

$modules
