#!/bin/bash
make DESTDIR=AppDir install

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

if [ ! -f linuxdeploy-x86_64.AppImage ]; then
  wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
chmod +x linuxdeploy-x86_64.AppImage
VERSION=$(git describe)
./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage $(find AppDir/ -name "lib*.so" | sed -e 's/^/ -l /' | tr -d '\n') $(find AppDir/ -name "vkdt*" -executable | sed -e 's/^/ -e /' | tr -d '\n')

if [ ! -f appimagetool-x86_64.AppImage ]; then
  wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
fi
chmod +x appimagetool-x86_64.AppImage
./appimagetool-x86_64.AppImage --sign --sign-key 2E42D54C99A903BF2F21C43CFC8D1E1D8ED4281C AppDir vkdt-${VERSION}-x86_64.AppImage 
rm -rf AppDir/
