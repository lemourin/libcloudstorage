TEMPLATE = app
CONFIG += c++11
QT =

TOOLCHAIN_PATH = $$_PRO_FILE_PWD_/../../../dependencies/win32

INCLUDEPATH = src

LIBS += \
    -L$$TOOLCHAIN_PATH/lib \
    -llibcloudbrowser

SOURCES += \
    ../src/main.cpp
