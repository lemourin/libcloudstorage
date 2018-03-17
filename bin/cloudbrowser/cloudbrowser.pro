TEMPLATE = app
CONFIG += c++11
QT =

MSVC_TOOLCHAIN_PATH = $$(MSVC_TOOLCHAIN_PATH)

isEmpty(MSVC_TOOLCHAIN_PATH) {
    error(MSVC_TOOLCHAIN_PATH environment variable not set.)
}

INCLUDEPATH = src

LIBS += \
    -L$$MSVC_TOOLCHAIN_PATH/lib \
    -llibcloudbrowser

SOURCES += \
    src/main.cpp

winrt {
    DISTFILES += \
        winrt/assets/logo_44x44.png \
        winrt/assets/logo_150x150.png \
        winrt/assets/logo_620x300.png \
        winrt/assets/logo_store.png

    WINRT_MANIFEST.name = "Cloud Browser"
    WINRT_MANIFEST.background = lightBlue
    WINRT_MANIFEST.logo_store = winrt/assets/logo_store.png
    WINRT_MANIFEST.logo_small = winrt/assets/logo_44x44.png
    WINRT_MANIFEST.logo_150x150 = winrt/assets/logo_150x150.png
    WINRT_MANIFEST.logo_620x300 = winrt/assets/logo_620x300.png
    WINRT_MANIFEST.capabilities = \
        codeGeneration internetClient internetClientServer \
        privateNetworkClientServer
}
