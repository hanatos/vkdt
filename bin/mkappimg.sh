#!/bin/bash

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
# this is necessary to make it run on some systems that don't ship for instance libjpeg.
# if we don't set the path it doesn't find *any* library (system or internal).
# on the other hand some systems have incompatible libc and will try to pick up the
# internal library and then fail completely.
export LD_LIBRARY_PATH=${HERE}/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}
${HERE}/usr/bin/vkdt $@
EOF
chmod +x AppDir/AppRun

if [ ! -f linuxdeploy-x86_64.AppImage ]; then
  wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
chmod +x linuxdeploy-x86_64.AppImage
export VERSION=$(git describe)
export ARCH=x86_64
export DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1
./linuxdeploy-x86_64.AppImage --appdir AppDir -i vkdt.png --output appimage $(find AppDir/ -name "lib*.so" | sed -e 's/^/ -l /' | tr -d '\n') \
-e AppDir/usr/bin/vkdt
rm -rf AppDir
