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
	$DepencyWalker = ''
)

$libraryItem = Get-Item $Library
$libraryDirectory = $libraryItem.Directory.FullName

If( $DepencyWalker -eq '') {
	$paths = @( $env:ProgramFiles )
	If( $env:ProgramW6432 -ne $null) {
		$paths+= $env:ProgramW6432
	}
	If( (${env:ProgramFiles(x86)} -ne $null) -and (${env:ProgramFiles(x86)} -ne $env:ProgramFiles ) ) {
		$paths+= ${env:ProgramFiles(x86)}
	}
	Write-Verbose "Search for depends.exe in $paths"
	$DepencyWalker = ($paths | ForEach-Object {Get-ChildItem -Path $_ -Filter depends.exe -Recurse } | Select-Object -First 1).FullName
}

Write-Verbose "DepencyWalker is $DepencyWalker"

$oldPath = $env:Path
$newPath = $env:Path
If( $SearchPath -ne '') {
	$newPath = "$SearchPath;$newPath"
}
$newPath = "$libraryDirectory;$newPath"

$output = [System.IO.Path]::GetTempFileName()

Try {
	$env:Path = $newPath
	
	Start-Process -Wait $DepencyWalker -ArgumentList @( "/c", "/f:1", "/oc", $output, $($libraryItem.FullName) )
}
Finally {
	$env:Path = $oldPath
}

$modules = Import-Csv $output

Remove-Item $output

$modules
