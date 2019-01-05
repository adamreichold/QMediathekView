CONFIG += c++11

CONFIG(release, debug|release) {
    QMAKE_CFLAGS += -flto
    QMAKE_CXXFLAGS += -flto
    QMAKE_LFLAGS += -flto
}

QT += core concurrent xml sql network gui widgets

CONFIG += link_pkgconfig
PKGCONFIG += liblzma

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
    application.h

target.path = /usr/bin

launcher.files = $${TARGET}.desktop
launcher.path = /usr/share/applications

icon.files = $${TARGET}.svg
icon.path = /usr/share/icons/hicolor/scalable/apps

INSTALLS += target launcher icon
