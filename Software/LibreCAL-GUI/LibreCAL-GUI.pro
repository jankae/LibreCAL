CONFIG += c++17

QT += core gui widgets charts

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
        Util/util.cpp \
        appwindow.cpp \
        caldevice.cpp \
        informationbox.cpp \
        main.cpp \
        touchstone.cpp \
        usbdevice.cpp

LIBS += -lusb-1.0
unix:LIBS += -L/usr/lib/
win32:LIBS += -L"$$_PRO_FILE_PWD_" # Github actions placed libusb here
osx:INCPATH += /usr/local/include
osx:LIBS += $(shell pkg-config --libs libusb-1.0)

DEFINES += FW_MAJOR=0 FW_MINOR=1 FW_PATCH=0 FW_SUFFIX=""#\\"\"-alpha.2\\"\"

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    Util/util.h \
    appwindow.h \
    caldevice.h \
    informationbox.h \
    touchstone.h \
    usbdevice.h

FORMS += \
    main.ui
