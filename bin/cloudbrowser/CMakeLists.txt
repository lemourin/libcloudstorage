set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(cloudbrowser WIN32)

target_sources(cloudbrowser PRIVATE
    src/AndroidUtility.cpp
    src/AndroidUtility.h
    src/CloudContext.cpp
    src/CloudContext.h
    src/CloudItem.cpp
    src/CloudItem.h
    src/DesktopUtility.cpp
    src/DesktopUtility.h
    src/Exec.cpp
    src/Exec.h
    src/File.cpp
    src/File.h
    src/FileDialog.cpp
    src/FileDialog.h
    src/IPlatformUtility.h
    src/main.cpp
    src/MpvPlayer.cpp
    src/MpvPlayer.h
    src/Request/CloudRequest.cpp
    src/Request/CloudRequest.h
    src/Request/CopyItem.cpp
    src/Request/CopyItem.h
    src/Request/CreateDirectory.cpp
    src/Request/CreateDirectory.h
    src/Request/DeleteItem.cpp
    src/Request/DeleteItem.h
    src/Request/DownloadItem.cpp
    src/Request/DownloadItem.h
    src/Request/GetThumbnail.cpp
    src/Request/GetThumbnail.h
    src/Request/GetUrl.cpp
    src/Request/GetUrl.h
    src/Request/ListDirectory.cpp
    src/Request/ListDirectory.h
    src/Request/MoveItem.cpp
    src/Request/MoveItem.h
    src/Request/RenameItem.cpp
    src/Request/RenameItem.h
    src/Request/UploadItem.cpp
    src/Request/UploadItem.h
    src/WinRTUtility.cpp
    src/WinRTUtility.h
    resources.qrc
    ../../src/Utility/HttpServer.cpp
    ../../src/Utility/GenerateThumbnail.cpp
)

set_target_properties(cloudbrowser
    PROPERTIES
        CXX_STANDARD 17
)

target_include_directories(cloudbrowser PRIVATE "src")

target_link_libraries(cloudbrowser PRIVATE 
    cloudstorage
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::QuickControls2
    Qt5::Widgets
    Qt5::Qml
    Qt5::Svg
    Threads::Threads
)

cloudstorage_target_link_library(cloudbrowser jsoncpp)

if(ffmpeg_FOUND)
    target_compile_definitions(cloudbrowser PRIVATE WITH_THUMBNAILER)
    cloudstorage_target_link_library(cloudbrowser ffmpeg)
endif()

find_package(Qt5WebView CONFIG)
if(Qt5WebView_FOUND)
    target_compile_definitions(cloudbrowser PRIVATE WITH_QTWEBVIEW)
    target_link_libraries(cloudbrowser PRIVATE Qt5::WebView)
endif()

cloudstorage_find_library(mpv INCLUDE mpv/client.h NAMES mpv)
if(mpv_LIBRARY)
    target_compile_definitions(cloudbrowser PRIVATE WITH_MPV)
    cloudstorage_target_link_library(cloudbrowser mpv)
endif()

if(MSVC)
    target_compile_options(cloudbrowser PRIVATE /wd4996 /wd4251 /wd4275 /Zc:__cplusplus /EHsc)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    if(MSVC)
        target_compile_options(cloudbrowser PRIVATE /ZW)
    endif()
    target_link_libraries(cloudbrowser PRIVATE Qt5::CorePrivate)
    set(ASSET_FILES
        winrt/Assets/logo_44x44.png
        winrt/Assets/logo_150x150.png
        winrt/Assets/logo_620x300.png
        winrt/Assets/logo_store.png
    )
    set(RESOURCE_FILES
        winrt/Package.appxmanifest
        winrt/cloudbrowser-winrt_TemporaryKey.pfx
    )
    set_property(SOURCE ${RESOURCE_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
    set_property(SOURCE ${ASSET_FILES} ${RESOURCE_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
    set_property(SOURCE ${ASSET_FILES} PROPERTY VS_DEPLOYMENT_LOCATION "Assets")
    target_sources(cloudbrowser PRIVATE ${RESOURCE_FILES} ${ASSET_FILES})
endif()

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(cloudbrowser PRIVATE QT_STATICPLUGIN)

    find_path(KIRIGAMI_PLUGIN_SOURCE_PATH NAMES kirigamiplugin.h)
    target_include_directories(cloudbrowser PRIVATE ${KIRIGAMI_PLUGIN_SOURCE_PATH})

    macro(cloudstorage_find_plugin plugin path_suffix plugin_name)
        cloudstorage_find_library(${plugin} LIBRARY_PATH_SUFFIXES ${path_suffix} NAMES ${plugin_name} DEBUG_NAMES ${plugin_name}d)
    endmacro()

    cloudstorage_find_plugin(qtquick2_plugin qml/QtQuick.2 qtquick2plugin)
    cloudstorage_find_plugin(qtquickcontrols2_plugin qml/QtQuick/Controls.2 qtquickcontrols2plugin)
    cloudstorage_find_plugin(qtquickcontrols2materialstyle_plugin qml/QtQuick/Controls.2/Material qtquickcontrols2materialstyleplugin)
    cloudstorage_find_plugin(qtquicklayouts_plugin qml/QtQuick/Layouts qquicklayoutsplugin)
    cloudstorage_find_plugin(qtquicktemplates2_plugin qml/QtQuick/Templates.2 qtquicktemplates2plugin)
    cloudstorage_find_plugin(qtwindow_plugin qml/QtQuick/Window.2 windowplugin)
    cloudstorage_find_plugin(qtgraphicaleffects_plugin qml/QtGraphicalEffects qtgraphicaleffectsplugin)
    cloudstorage_find_plugin(qtgraphicaleffectsprivate_plugin qml/QtGraphicalEffects/private qtgraphicaleffectsprivate)
    cloudstorage_find_plugin(qtqmlmodels_plugin qml/QtQml/Models.2 modelsplugin)
    cloudstorage_find_plugin(qtqml_plugin qml/QtQml qmlplugin)
    cloudstorage_find_plugin(qtlabsplatform_plugin qml/Qt/labs/platform qtlabsplatformplugin)
    cloudstorage_find_plugin(qtsvg_plugin plugins/imageformats qsvg)
    cloudstorage_find_plugin(qtsvgicon_plugin plugins/iconengines qsvgicon)
    cloudstorage_find_library(kirigami_plugin LIBRARY_PATH_SUFFIXES lib/qml/org/kde/kirigami.2 qml/org/kde/kirigami.2 NAMES kirigamiplugin)

    cloudstorage_target_link_library(cloudbrowser qtquick2_plugin)
    cloudstorage_target_link_library(cloudbrowser qtquickcontrols2_plugin)
    cloudstorage_target_link_library(cloudbrowser qtquicklayouts_plugin)
    cloudstorage_target_link_library(cloudbrowser qtquicktemplates2_plugin)
    cloudstorage_target_link_library(cloudbrowser qtgraphicaleffects_plugin)
    cloudstorage_target_link_library(cloudbrowser qtgraphicaleffectsprivate_plugin)
    cloudstorage_target_link_library(cloudbrowser qtquickcontrols2materialstyle_plugin)
    if(qtqml_plugin_LIBRARY)
        cloudstorage_target_link_library(cloudbrowser qtqml_plugin)
    endif()
    cloudstorage_target_link_library(cloudbrowser qtlabsplatform_plugin)
    cloudstorage_target_link_library(cloudbrowser qtwindow_plugin)
    cloudstorage_target_link_library(cloudbrowser qtqmlmodels_plugin)
    cloudstorage_target_link_library(cloudbrowser qtsvg_plugin)
    cloudstorage_target_link_library(cloudbrowser qtsvgicon_plugin)
    cloudstorage_target_link_library(cloudbrowser kirigami_plugin)

    if(Qt5WebView_FOUND)
        cloudstorage_find_plugin(qtwebview_plugin qml/QtWebView declarative_webview)
        cloudstorage_target_link_library(cloudbrowser qtwebview_plugin)
    endif()

    find_package(Qt5 OPTIONAL_COMPONENTS QmlWorkerScript)
    if(Qt5QmlWorkerScript_FOUND)
        target_link_libraries(cloudbrowser PRIVATE Qt5::QmlWorkerScript)
    endif()

endif()

install(TARGETS cloudbrowser)
