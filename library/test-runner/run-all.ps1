
$scriptPath = split-path -parent $MyInvocation.MyCommand.Definition

$testRunnerExe = "$scriptPath\bin\windows\msvc-release-x64\test-runner.exe"

$oldEnc = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Output "Retrieving files..."

$allFiles = (-join (git ls-files -z)).Split([char]0) | Where-Object { $_ -ne "" } | Where-Object { Test-Path -Type Leaf -Path $_ }

Write-Output "Scanning all $($allFiles.Length) files..."

$testFiles = $allFiles | Where-Object { Select-String -Path $_ -Pattern "pmath>" -SimpleMatch -Quiet }

$total = $testFiles.Length
Write-Output "Running inline tests in $total files..."

$consoleWidth = 0
$consoleWidth = [Console]::BufferWidth

$fails = 0
$passes = 0
$i = 0
foreach ($file in $testFiles) {
	$filePath = "$pwd\$file"
	
	Write-Progress -Activity "Running inline tests" -Status "[$i / $total] $file ..." -PercentComplete ($i * 100 / $total)
	$i++;
	
	Write-Host -NoNewLine ("[  ] $file ...".PadRight($consoleWidth - 5))
	
	$psi = New-Object System.Diagnostics.ProcessStartInfo
	$psi.FileName               = $testRunnerExe
	$psi.Arguments              = "`"$filePath`""
	$psi.RedirectStandardOutput = $true
	$psi.RedirectStandardError  = $true
	$psi.UseShellExecute        = $false
	$psi.CreateNoWindow         = $true
	
	$proc = New-Object System.Diagnostics.Process
	$proc.StartInfo = $psi
	$proc.Start() | Out-Null
	
	$stdout = $proc.StandardOutput.ReadToEnd()
	$stderr = $proc.StandardError.ReadToEnd()

	$proc.WaitForExit()
	$exit = $proc.ExitCode
	if($exit -ne 0) {
		Write-Host "FAIL`r[✗"
		$fails++
		if($stdout -ne "") {
			Write-Host $stdout
		}
		if($stderr -ne "") {
			Write-Warning $stderr
		}
	} else {
		Write-Host "PASS`r[✓"
		$passes++
	}
}

Write-Progress -Activity "Run Tests" -Completed

if($passes -eq $total) {
	Write-Output "All $total tests passed."
} else {
	Write-Warning "$fails tests FAILED. $passes tests passed."
}

[Console]::OutputEncoding = $oldEnc

if($passes -eq $total) {
	exit 0
} else {
	exit 1
}
