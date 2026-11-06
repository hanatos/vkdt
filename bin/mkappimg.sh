#!/bin/bash
arch=$1
if [ ${arch}=="x86-64" ]; then
  arch="x86_64"
fi
if [ ${arch}=="armv8-a" ]; then
  arch="aarch64"
fi

# install vkdt to AppDir
make DESTDIR=AppDir install

# create fake desktop entry + icon because apparently it's needed
mkdir -p AppDir/usr/share/applications
cat > AppDir/usr/share/applications/vkdt.desktop << EOF
[Desktop Entry]
Name=vkdt
Comment=gpu accelerated photography workflow toolbox
Exec=vkdt
Icon=vkdt
Terminal=false
Type=Application
Categories=Graphics;
StartupNotify=false
EOF
mkdir -p AppDir/usr/share/icons/
cp vkdt.png AppDir/usr/share/icons/

cat > AppDir/AppRun << 'EOF'
#!/bin/bash
HERE=$(dirname $(realpath ${0}))
export LD_LIBRARY_PATH=${HERE}/usr/lib:${LD_LIBRARY_PATH}
${HERE}/usr/bin/vkdt $@
EOF
chmod +x AppDir/AppRun

if [ ! -f linuxdeploy-${arch}.AppImage ]; then
  wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${arch}.AppImage
fi
chmod +x linuxdeploy-${arch}.AppImage
export VERSION=$(git describe)
export ARCH=${arch}
export NO_STRIP=1
export DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1
echo "adding library $(find AppDir/ -name "lib*.so" | sed -e 's/^/ --library=/' | tr -d '\n') "
./linuxdeploy-${arch}.AppImage --appdir=AppDir --icon-file=vkdt.png --output=appimage \
$(find AppDir/ -name "lib*.so" | sed -e 's/^/ --library=/' | tr -d '\n') \
--executable=AppDir/usr/bin/vkdt \
--deploy-deps-only=AppDir/usr/lib/vkdt
rm -rf AppDir
