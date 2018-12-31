#!/usr/bin/env bash

cd "$(dirname "$0")"

CONTRIB_DIR=$PWD/../../contrib/x86_64-linux-gnu

mkdir -p cloudbrowser-appimage
cd cloudbrowser-appimage

mkdir usr/share/icons/hicolor/128x128 -p
cp ../../../bin/cloudbrowser/resources/cloud.png \
  usr/share/icons/hicolor/128x128/cloudbrowser.png
ln -sf usr/share/icons/hicolor/128x128/cloudbrowser.png cloudbrowser.png 

mkdir usr/share/applications -p
cp ../cloudbrowser.desktop usr/share/applications
ln -sf usr/share/applications/cloudbrowser.desktop cloudbrowser.desktop

mkdir usr/bin -p
ln -sf /tmp/cloudbrowser-qt.conf usr/bin/qt.conf

mkdir usr/optional/libgcc -p
cp /lib/x86_64-linux-gnu/libgcc_s.so.1 usr/optional/libgcc

mkdir usr/optional/libstdc++ -p
cp /usr/lib/x86_64-linux-gnu/libstdc++.so.6 usr/optional/libstdc++

wget -nc \
  https://github.com/darealshinji/AppImageKit-checkrt/releases/download/continuous/exec-x86_64.so \
  -O usr/optional/exec.so

cp ../AppRun .

cp \
  $CONTRIB_DIR/lib \
  $CONTRIB_DIR/libexec \
  $CONTRIB_DIR/openssl \
  $CONTRIB_DIR/phrasebooks \
  $CONTRIB_DIR/plugins \
  $CONTRIB_DIR/qml \
  $CONTRIB_DIR/resources \
  $CONTRIB_DIR/translations \
  usr -r

rm -f usr/lib/vlc/plugins/plugins.dat
rm -f usr/lib/*.a usr/lib/*.la usr/lib/*.prl

cd ..

mkdir build -p
cd build

libcryptopp_CFLAGS="-I${CONTRIB_DIR}/include" libcryptopp_LIBS=-lcryptopp \
PKG_CONFIG_LIBDIR=$CONTRIB_DIR/lib/pkgconfig \
  ../../../configure \
  --prefix=$PWD/../cloudbrowser-appimage/usr \
  CXXFLAGS="-I${CONTRIB_DIR}/include $CXXFLAGS" \
  LDFLAGS="-L${CONTRIB_DIR}/lib $LDFLAGS"

LD_LIBRARY_PATH=$CONTRIB_DIR/lib/ make -j 4 install

wget -nc "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

./appimagetool-x86_64.AppImage ../cloudbrowser-appimage
