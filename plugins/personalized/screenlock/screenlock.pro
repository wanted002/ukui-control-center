#-------------------------------------------------
#
# Project created by QtCreator 2019-06-26T08:25:40
#
#-------------------------------------------------

QT       += widgets xml

TEMPLATE = lib
CONFIG += plugin

TARGET = $$qtLibraryTarget(screenlock)
DESTDIR = ../../../pluginlibs

include(../../../env.pri)
include($$PROJECT_COMPONENTSOURCE/switchbutton.pri)
include($$PROJECT_COMPONENTSOURCE/flowlayout.pri)
include($$PROJECT_COMPONENTSOURCE/maskwidget.pri)

INCLUDEPATH   +=  \
                 $$PROJECT_ROOTDIR \
                 $$PROJECT_COMPONENTSOURCE \

LIBS += -L/usr/lib/ -lgsettings-qt


##加载gio库和gio-unix库
#CONFIG        += link_pkgconfig \
#                 C++11
#PKGCONFIG     += gio-2.0 \
#                 gio-unix-2.0

#DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        bgfileparse.cpp \
        buildpicunitsworker.cpp \
        pictureunit.cpp \
        screenlock.cpp

HEADERS += \
        bgfileparse.h \
        buildpicunitsworker.h \
        pictureunit.h \
        screenlock.h

FORMS += \
        screenlock.ui
