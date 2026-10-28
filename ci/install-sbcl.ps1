param([Parameter(Mandatory = $true)][string]$Destination)

$ErrorActionPreference = 'Stop'
if (Test-Path $Destination) {
    throw "SBCL staging directory already exists: $Destination"
}
New-Item -ItemType Directory -Path $Destination | Out-Null
$installer = Join-Path $Destination 'sbcl.msi'
$extract = Join-Path $Destination 'extracted'
$log = Join-Path $Destination 'install.log'
$url = 'https://downloads.sourceforge.net/project/sbcl/sbcl/2.6.9/sbcl-2.6.9-x86-64-windows-binary.msi'
Invoke-WebRequest -Uri $url -OutFile $installer
$expected = '2be695e44a69ff29f01cf7980f67472cb383e75e9292d5fcd27cf5418b871226'
if ((Get-FileHash -Algorithm SHA256 $installer).Hash.ToLowerInvariant() -ne $expected) {
    throw 'SBCL installer SHA256 does not match the pinned binary'
}
# Administrative extraction keeps this pinned runtime private to the CI job.
$arguments = "/a `"$installer`" /qn /norestart TARGETDIR=`"$extract`" /l*v `"$log`""
$process = Start-Process -FilePath msiexec.exe -ArgumentList $arguments -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "SBCL extraction failed with exit $($process.ExitCode); see $log"
}
$executables = @(Get-ChildItem $extract -Recurse -Filter sbcl.exe -File)
$cores = @(Get-ChildItem $extract -Recurse -Filter sbcl.core -File)
if ($executables.Count -ne 1 -or $cores.Count -ne 1) {
    throw 'Expected exactly one sbcl.exe and sbcl.core in the extracted installer'
}
$bin = $executables[0].Directory.FullName
$env:SBCL_HOME = $cores[0].Directory.FullName
$env:PATH = "$bin;$env:PATH"
if ($env:GITHUB_PATH) {
    Add-Content -Path $env:GITHUB_PATH -Value $bin -Encoding utf8
}
if ($env:GITHUB_ENV) {
    Add-Content -Path $env:GITHUB_ENV -Value "SBCL_HOME=$env:SBCL_HOME" -Encoding utf8
}
& $executables[0].FullName --version
if ($LASTEXITCODE -ne 0) {
    throw "SBCL failed to launch with exit $LASTEXITCODE"
}
