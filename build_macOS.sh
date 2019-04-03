#!/bin/bash

set -o errexit -o nounset

# Hold on to current directory
project_dir=$(pwd)

# Output macOS version
sw_vers

# Update platform
echo "Updating platform..."
brew update

# Install p7zip for packaging
brew install p7zip

# Install npm appdmg if you want to create custom dmg files with it
# npm install -g appdmg

# Install Qt
echo "Installing Qt..."
brew install qt

# Add Qt binaries to path
PATH=/usr/local/opt/qt/bin/:${PATH}

# Build your app
echo "Building CommerciumQT..."
cd ${project_dir}
qmake -config release
make

# Build and run your tests here

# Package your app
echo "Packaging CommerciuMQT..."
cd ${project_dir}/build/macOS/clang/x86_64/release/

# Remove build directories that you don't want to deploy
rm -rf moc
rm -rf obj
rm -rf qrc

echo "Creating dmg archive..."
macdeployqt CommerciumQT.app -qmldir=../../../../../src -dmg
mv CommerciumQT.dmg "CommerciumQT_${TAG_NAME}.dmg"

# You can use the appdmg command line app to create your dmg file if
# you want to use a custom background and icon arrangement. I'm still
# working on this for my apps, myself. If you want to do this, you'll
# remove the -dmg option above.
# appdmg json-path CommerciumQT_${TRAVIS_TAG}.dmg

# Copy other project files
cp "${project_dir}/README.md" "README.md"
cp "${project_dir}/LICENSE" "LICENSE"
cp "${project_dir}/Qt License" "Qt License"

echo "Packaging zip archive..."
7z a CommerciumQT_${TAG_NAME}_macos.zip "CommerciumQT_${TAG_NAME}.dmg" "README.md" "LICENSE" "Qt License"

echo "Done!"

exit 0
