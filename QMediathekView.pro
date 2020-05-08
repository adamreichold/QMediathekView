CONFIG += c++11

CONFIG(debug, debug|release) {
    internals.target = $${OUT_PWD}/internals/debug/libinternals.a
    internals.commands = LZMA_API_STATIC=1 cargo build --manifest-path "$${PWD}/internals/Cargo.toml" --target-dir "$${OUT_PWD}/internals"
}

CONFIG(release, debug|release) {
    internals.target = $${OUT_PWD}/internals/release/libinternals.a
    internals.commands = LZMA_API_STATIC=1 cargo build --manifest-path "$${PWD}/internals/Cargo.toml" --target-dir "$${OUT_PWD}/internals" --release
}

internals.CONFIG = phony
QMAKE_EXTRA_TARGETS += internals
PRE_TARGETDEPS += $${internals.target}
LIBS += $${internals.target} -ldl -lz -lssl -lcrypto

QT += core network gui widgets

TARGET = QMediathekView
TEMPLATE = app

SOURCES += \
    settings.cpp \
    database.cpp \
    model.cpp \
    miscellaneous.cpp \
    mainwindow.cpp \
    downloaddialog.cpp \
    settingsdialog.cpp \
    application.cpp

HEADERS += \
    settings.h \
    schema.h \
    database.h \
    model.h \
    miscellaneous.h \
    mainwindow.h \
    downloaddialog.h \
    settingsdialog.h \
    application.h

target.path = /usr/bin

launcher.files = $${TARGET}.desktop
launcher.path = /usr/share/applications

icon.files = $${TARGET}.svg
icon.path = /usr/share/icons/hicolor/scalable/apps

appdata.files = $${TARGET}.appdata.xml
appdata.path = /usr/share/metainfo

INSTALLS += target launcher icon appdata
