CONFIG += c++11

CONFIG(release, debug|release) {
    QMAKE_CFLAGS += -flto
    QMAKE_CXXFLAGS += -flto
    QMAKE_LFLAGS += -flto
}

QT += core concurrent sql network gui widgets

CONFIG += link_pkgconfig
PKGCONFIG += libtorrent-rasterbar liblzma

TARGET = QMediathekView
TEMPLATE = app

SOURCES += \
    settings.cpp \
    parser.cpp \
    database.cpp \
    model.cpp \
    miscellaneous.cpp \
    mainwindow.cpp \
    downloaddialog.cpp \
    settingsdialog.cpp \
    torrentsession.cpp \
    application.cpp

HEADERS += \
    settings.h \
    schema.h \
    parser.h \
    database.h \
    model.h \
    miscellaneous.h \
    mainwindow.h \
    downloaddialog.h \
    settingsdialog.h \
    torrentsession.h \
    application.h

target.path = /usr/bin

launcher.files = $${TARGET}.desktop
launcher.path = /usr/share/applications

INSTALLS += target launcher
