QT       += core gui sql network concurrent help

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    commandline.cpp \
    constvalues.cpp \
    directparser.cpp \
    directrunner.cpp \
    helpbrowser.cpp \
    main.cpp \
    mainwindow.cpp \
    manualmode.cpp \
    protmanager.cpp \
    rrparam.cpp \
    stepwgt.cpp \
    textsearcher.cpp \
    widgetinfo.cpp \
    jsonreceiver.cpp

HEADERS += \
    commandline.h \
    constvalues.h \
    dialogwgt.h \
    directparser.h \
    directrunner.h \
    helpbrowser.h \
    mainwindow.h \
    manualmode.h \
    protmanager.h \
    rrparam.h \
    stepwgt.h \
    textsearcher.h \
    widgetinfo.h \
    jsonreceiver.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc

DISTFILES += \
    PRIS.pro.user
