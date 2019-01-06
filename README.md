cmm-qt-wallet, sapling compatible wallet and full node for Commercium that runs on Linux, Windows and macOS. Based on zec-qt-wallet


# Installation

### Linux

If you are on Debian/Ubuntu, please download the `.deb` package and install it.
```
sudo dpkg -i linux-deb-cmm-qt-wallet-v0.5.6.deb
sudo apt install -f
```

Or you can download and run the binaries directly.
```
tar -xvf cmm-qt-wallet-v0.5.6.tar.gz
./cmm-qt-wallet-v0.5.6/cmm-qt-wallet
```

### Windows
Download and run the .msi installer and follow the prompts. Alternately, you can download the release binary, unzip it and double click on cmm-qt-wallet to start.

### macOS
Double-click on the .dmg file to open it, and drag cmm-qt-wallet on to the Applications link to install.

## commerciumd
cmm-qt-wallet needs a Commercium node running commerciumd. If you already have a commerciumd node running, cmm-qt-wallet will connect to it. 

If you don't have one, cmm-qt-wallet will start its embedded commerciumd node. 

Additionally, if this is the first time you're running cmm-qt-wallet or a commerciumd daemon, cmm-qt-wallet will download the commercium params (~1.7 GB) and configure `commercium.conf` for you. 

Pass `--no-embedded` to disable the embedded commerciumd and force cmm-qt-wallet to connect to an external node.

## Compiling from source
cmm-qt-wallet is written in C++ 14, and can be compiled with g++/clang++/visual c++. It also depends on Qt5, which you can get from [here](https://www.qt.io/download). Note that if you are compiling from source, you won't get the embedded commerciumd by default. You can either run an external commerciumd, or compile commerciumd as well. 

See detailed build instructions [on the wiki](https://github.com/CommerciumBlockchain/cmm-qt-wallet/wiki/Compiling-from-source-code)

### Building on Linux

```
git clone https://github.com/CommerciumBlockchain/cmm-qt-wallet.git
cd cmm-qt-wallet
/path/to/qt5/bin/qmake cmm-qt-wallet.pro CONFIG+=debug
make -j$(nproc)

./cmm-qt-wallet
```

### Building on Windows
You need Visual Studio 2017 (The free C++ Community Edition works just fine). 

From the VS Tools command prompt
```
git clone https://github.com/CommerciumBlockchain/cmm-qt-wallet.git
cd cmm-qt-wallet
c:\Qt5\bin\qmake.exe cmm-qt-wallet.pro -spec win32-msvc CONFIG+=debug
nmake

debug\cmm-qt-wallet.exe
```

To create the Visual Studio project files so you can compile and run from Visual Studio:
```
c:\Qt5\bin\qmake.exe cmm-qt-wallet.pro -tp vc CONFIG+=debug
```

### Building on macOS
You need to install the Xcode app or the Xcode command line tools first, and then install Qt. 

```
git clone https://github.com/CommerciumBlockchain/cmm-qt-wallet.git
cd cmm-qt-wallet
/path/to/qt5/bin/qmake cmm-qt-wallet.pro CONFIG+=debug
make

./cmm-qt-wallet.app/Contents/MacOS/cmm-qt-wallet
```

### [Troubleshooting Guide & FAQ](https://github.com/CommerciumBlockchain/cmm-qt-wallet/wiki/Troubleshooting-&-FAQ)
Please read the [troubleshooting guide](https://github.com/CommerciumBlockchain/cmm-qt-wallet/wiki/Troubleshooting-&-FAQ) for common problems and solutions.
For support or other questions, tweet at [@cmmqtwallet](https://twitter.com/cmmqtwallet) or [file an issue](https://github.com/CommerciumBlockchain/cmm-qt-wallet/issues).

_PS: cmm-qt-wallet is NOT an official wallet, and is not affiliated with the Zerocoin Electric Coin Company in any way._
# cmm-qt-wallet
