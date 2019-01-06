# Unified build script for Windows, Linux and Mac builder. Run on a Windows machine inside powershell.
param (
    [Parameter(Mandatory=$true)][string]$version,
    [Parameter(Mandatory=$true)][string]$prev,
    [Parameter(Mandatory=$true)][string]$server,
    [Parameter(Mandatory=$true)][string]$winserver
)

Write-Host "[Initializing]"
Remove-Item -Force -ErrorAction Ignore ./artifacts/linux-binaries-cmm-qt-wallet-v$version.tar.gz
Remove-Item -Force -ErrorAction Ignore ./artifacts/linux-deb-cmm-qt-wallet-v$version.deb
Remove-Item -Force -ErrorAction Ignore ./artifacts/Windows-binaries-cmm-qt-wallet-v$version.zip
Remove-Item -Force -ErrorAction Ignore ./artifacts/Windows-installer-cmm-qt-wallet-v$version.msi
Remove-Item -Force -ErrorAction Ignore ./artifacts/macOS-cmm-qt-wallet-v$version.dmg

Remove-Item -Recurse -Force -ErrorAction Ignore ./bin
Remove-Item -Recurse -Force -ErrorAction Ignore ./debug
Remove-Item -Recurse -Force -ErrorAction Ignore ./release

# Create the version.h file and update README version number
Write-Output "#define APP_VERSION `"$version`"" > src/version.h
Get-Content README.md | Foreach-Object { $_ -replace "$prev", "$version" } | Out-File README-new.md
Move-Item -Force README-new.md README.md
Write-Host ""


Write-Host "[Building on Mac]"
bash src/scripts/mkmacdmg.sh --qt_path ~/Qt/5.11.1/clang_64/ --version $version --commercium_path ~/github/commercium 
if (! $?) {
    Write-Output "[Error]"
    exit 1;
}
Write-Host ""


Write-Host "[Building Linux + Windows]"
Write-Host -NoNewline "Copying files.........."
ssh $server "rm -rf /tmp/zqwbuild"
ssh $server "mkdir /tmp/zqwbuild"
scp -r src/ res/ ./cmm-qt-wallet.pro ./application.qrc ./LICENSE ./README.md ${server}:/tmp/zqwbuild/ | Out-Null
ssh $server "dos2unix -q /tmp/zqwbuild/src/scripts/mkrelease.sh" | Out-Null
ssh $server "dos2unix -q /tmp/zqwbuild/src/version.h"
Write-Host "[OK]"

ssh $server "cd /tmp/zqwbuild && APP_VERSION=$version PREV_VERSION=$prev bash src/scripts/mkrelease.sh"
if (!$?) {
    Write-Output "[Error]"
    exit 1;
}

New-Item artifacts -itemtype directory -Force         | Out-Null
scp    ${server}:/tmp/zqwbuild/artifacts/* artifacts/ | Out-Null
scp -r ${server}:/tmp/zqwbuild/release .              | Out-Null

Write-Host -NoNewline "Building Installer....."
ssh $winserver "Remove-Item -Path zqwbuild -Recurse"   | Out-Null
ssh $winserver "New-Item zqwbuild -itemtype directory" | Out-Null

# Note: For some mysterious reason, we can't seem to do a scp from here to windows machine. 
# So, we'll ssh to windows, and execute an scp command to pull files from here to there.
# Same while copying the built msi. A straight scp pull from windows to here doesn't work,
# so we ssh to windows, and then scp push the file to here.
$myhostname = (hostname) | Out-String -NoNewline
Remove-Item -Path /tmp/zqwbuild -Recurse -ErrorAction Ignore | Out-Null
New-Item    -Path /tmp/zqwbuild -itemtype directory          | Out-Null
Copy-Item src     /tmp/zqwbuild/ -Recurse
Copy-Item res     /tmp/zqwbuild/ -Recurse
Copy-Item release /tmp/zqwbuild/ -Recurse
ssh $winserver "scp -r ${myhostname}:/tmp/zqwbuild/* zqwbuild/"
ssh $winserver "cd zqwbuild ; src/scripts/mkwininstaller.ps1 -version $version" >/dev/null
if (!$?) {
    Write-Output "[Error]"
    exit 1;
}
ssh $winserver "scp zqwbuild/artifacts/* ${myhostname}:/tmp/zqwbuild/"
Copy-Item /tmp/zqwbuild/*.msi artifacts/
Write-Host "[OK]"

# Finally, test to make sure all files exist
Write-Host -NoNewline "Checking Build........."
if (! (Test-Path ./artifacts/linux-binaries-cmm-qt-wallet-v$version.tar.gz) -or
    ! (Test-Path ./artifacts/linux-deb-cmm-qt-wallet-v$version.deb) -or
    ! (Test-Path ./artifacts/Windows-binaries-cmm-qt-wallet-v$version.zip) -or
    ! (Test-Path ./artifacts/macOS-cmm-qt-wallet-v$version.dmg) -or 
    ! (Test-Path ./artifacts/Windows-installer-cmm-qt-wallet-v$version.msi) ) {
        Write-Host "[Error]"
        exit 1;
    }
Write-Host "[OK]"
