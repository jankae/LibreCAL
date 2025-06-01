CONFIG += c++17

QT += core gui widgets charts svg network

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        CustomWidgets/informationbox.cpp \
        CustomWidgets/siunitedit.cpp \
        CustomWidgets/touchstoneimport.cpp \
        Util/QMicroz/miniz.c \
        Util/QMicroz/qmicroz.cpp \
        Util/QMicroz/tools.cpp \
        Util/usbinbuffer.cpp \
        Util/util.cpp \
        about.cpp \
        appwindow.cpp \
        caldevice.cpp \
        main.cpp \
        savable.cpp \
        touchstone.cpp \
        touchstoneimportdialog.cpp \
        unit.cpp \
        usbdevice.cpp

LIBS += -lusb-1.0
unix:LIBS += -L/usr/lib/
win32:LIBS += -L"$$_PRO_FILE_PWD_" # Github actions placed libusb here
osx:INCPATH += /usr/local/include
osx:LIBS += -L/usr/local/lib

mac{
	QT_CONFIG -= no-pkg-config
	CONFIG += link_pkgconfig
	PKGCONFIG += libusb-1.0
    ICON = resources/librecal.icns
}

# libusb-1.0.23 shall be extracted in same directory as this file
windows{
    INCLUDEPATH += ./libusb-1.0.23/include
    DEFINES += QMICROZ_LIBRARY
    contains(QMAKE_CC, gcc){
        # MingW64 libusb static lib
        LIBS += -L"$$_PRO_FILE_PWD_"/libusb-1.0.23/MinGW64/static
    }
    contains(QMAKE_CC, cl){
        # Visual Studio 2019 64bits libusb DLL
        LIBS = -L"$$_PRO_FILE_PWD_"/libusb-1.0.23/MS64/dll
        LIBS += -llibusb-1.0
    }
}

REVISION = $$system(git rev-parse HEAD)
DEFINES += GITHASH=\\"\"$$REVISION\\"\"
DEFINES += FW_MAJOR=0 FW_MINOR=3 FW_PATCH=0 FW_SUFFIX=""#\\"\"-alpha.2\\"\"

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    CustomWidgets/informationbox.h \
    CustomWidgets/siunitedit.h \
    CustomWidgets/touchstoneimport.h \
    Util/QMicroz/miniz.h \
    Util/QMicroz/qmicroz.h \
    Util/QMicroz/tools.h \
    Util/qpointervariant.h \
    Util/usbinbuffer.h \
    Util/util.h \
    about.h \
    appwindow.h \
    caldevice.h \
    json.hpp \
    savable.h \
    touchstone.h \
    touchstoneimportdialog.h \
    unit.h \
    usbdevice.h

FORMS += \
    CustomWidgets/touchstoneimport.ui \
    aboutdialog.ui \
    factoryUpdateDialog.ui \
    main.ui \
    touchstoneimportdialog.ui

DISTFILES +=

RESOURCES += \
    resources/frontpanel.qrc
