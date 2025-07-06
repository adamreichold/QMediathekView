sudo apt-get install --yes --no-install-recommends qtbase5-dev qt5-qmake libqt5svg5-dev fuse

mkdir -p build
pushd build

qmake ..
make -j$(nproc)
make INSTALL_ROOT=../appdir install

popd

wget --continue --no-verbose "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt-continuous-x86_64.AppImage
unset QTDIR; unset QT_PLUGIN_PATH; unset LD_LIBRARY_PATH
VERSION=`git rev-parse --short HEAD` ./linuxdeployqt-continuous-x86_64.AppImage appdir/usr/share/applications/QMediathekView.desktop -appimage -extra-plugins=iconengines
