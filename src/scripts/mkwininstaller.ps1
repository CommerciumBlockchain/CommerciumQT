param (
    [Parameter(Mandatory=$true)][string]$version
)

$target="cmm-qt-wallet-v$version"

Remove-Item -Path release/wininstaller -Recurse -ErrorAction Ignore  | Out-Null
New-Item release/wininstaller -itemtype directory                    | Out-Null

Copy-Item release/$target/cmm-qt-wallet.exe release/wininstaller/
Copy-Item release/$target/LICENSE           release/wininstaller/
Copy-Item release/$target/README.md         release/wininstaller/
Copy-Item release/$target/commerciumd.exe        release/wininstaller/
Copy-Item release/$target/commercium-cli.exe     release/wininstaller/

Get-Content src/scripts/cmm-qt-wallet.wxs | ForEach-Object { $_ -replace "RELEASE_VERSION", "$version" } | Out-File -Encoding utf8 release/wininstaller/cmm-qt-wallet.wxs

candle.exe release/wininstaller/cmm-qt-wallet.wxs -o release/wininstaller/cmm-qt-wallet.wixobj 
if (!$?) {
    exit 1;
}

light.exe -ext WixUIExtension -cultures:en-us release/wininstaller/cmm-qt-wallet.wixobj -out release/wininstaller/cmm-qt-wallet.msi 
if (!$?) {
    exit 1;
}

New-Item artifacts -itemtype directory -Force | Out-Null
Copy-Item release/wininstaller/cmm-qt-wallet.msi ./artifacts/Windows-installer-$target.msi