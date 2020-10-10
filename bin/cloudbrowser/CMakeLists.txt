set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_library(cloudbrowser SHARED)
else()
    add_executable(cloudbrowser WIN32)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IOS_RESOURCES
        ios/launchscreen.storyboard
        resources/cloud.png
    )
    set(IOS_HEADERS
        src/IOSUtility.h
    )
    set(IOS_SOURCES
        src/IOSUtility.mm
    )
endif()

set(cloudbrowser_HEADERS
    src/AndroidUtility.h
    src/CloudContext.h
    src/CloudItem.h
    src/DesktopUtility.h
    src/Exec.h
    src/File.h
    src/FileDialog.h
    src/IPlatformUtility.h
    src/MpvPlayer.h
    src/Request/CloudRequest.h
    src/Request/CopyItem.h
    src/Request/CreateDirectory.h
    src/Request/DeleteItem.h
    src/Request/DownloadItem.h
    src/Request/GetThumbnail.h
    src/Request/GetUrl.h
    src/Request/ListDirectory.h
    src/Request/MoveItem.h
    src/Request/RenameItem.h
    src/Request/UploadItem.h
    src/WinRTUtility.h
    ${IOS_HEADERS}
)

set(cloudbrowser_SOURCES
    src/AndroidUtility.cpp
    src/CloudContext.cpp
    src/CloudItem.cpp
    src/DesktopUtility.cpp
    src/Exec.cpp
    src/File.cpp
    src/FileDialog.cpp
    src/main.cpp
    src/MpvPlayer.cpp
    src/Request/CloudRequest.cpp
    src/Request/CopyItem.cpp
    src/Request/CreateDirectory.cpp
    src/Request/DeleteItem.cpp
    src/Request/DownloadItem.cpp
    src/Request/GetThumbnail.cpp
    src/Request/GetUrl.cpp
    src/Request/ListDirectory.cpp
    src/Request/MoveItem.cpp
    src/Request/RenameItem.cpp
    src/Request/UploadItem.cpp
    src/WinRTUtility.cpp
    ${IOS_SOURCES}
)

set(cloudbrowser_RESOURCES
    resources/4shared_login.html
    resources/amazons3_login.html
    resources/bootstrap.min.css
    resources/bootstrap.min.js
    resources/cloud.png
    resources/cryptopp.png
    resources/curl.png
    resources/default_error.html
    resources/default_success.html
    resources/ffmpeg.png
    resources/gnu.png
    resources/icons/material/hdpi/applications-graphics.svg
    resources/icons/material/hdpi/audio-volume-high.svg
    resources/icons/material/hdpi/audio-volume-low.svg
    resources/icons/material/hdpi/audio-volume-medium.svg
    resources/icons/material/hdpi/audio-volume-muted.svg
    resources/icons/material/hdpi/audio-x-generic.svg
    resources/icons/material/hdpi/bookmarks.svg
    resources/icons/material/hdpi/configure.svg
    resources/icons/material/hdpi/dialog-apply.svg
    resources/icons/material/hdpi/dialog-cancel.svg
    resources/icons/material/hdpi/dialog-close.svg
    resources/icons/material/hdpi/document-decrypt.svg
    resources/icons/material/hdpi/document-edit.svg
    resources/icons/material/hdpi/document-open-remote.svg
    resources/icons/material/hdpi/document-save.svg
    resources/icons/material/hdpi/edit-add.svg
    resources/icons/material/hdpi/edit-cut.svg
    resources/icons/material/hdpi/edit-delete.svg
    resources/icons/material/hdpi/edit-paste-style.svg
    resources/icons/material/hdpi/folder-move.svg
    resources/icons/material/hdpi/folder-new.svg
    resources/icons/material/hdpi/folder-sync.svg
    resources/icons/material/hdpi/folder.svg
    resources/icons/material/hdpi/go-down.svg
    resources/icons/material/hdpi/go-next-symbolic-rtl.svg
    resources/icons/material/hdpi/go-next-symbolic.svg
    resources/icons/material/hdpi/go-next.svg
    resources/icons/material/hdpi/go-previous-symbolic-rtl.svg
    resources/icons/material/hdpi/go-previous-symbolic.svg
    resources/icons/material/hdpi/go-previous.svg
    resources/icons/material/hdpi/go-up.svg
    resources/icons/material/hdpi/gtk-file.svg
    resources/icons/material/hdpi/handle-left.svg
    resources/icons/material/hdpi/handle-right.svg
    resources/icons/material/hdpi/help-about.svg
    resources/icons/material/hdpi/image.svg
    resources/icons/material/hdpi/list-add.svg
    resources/icons/material/hdpi/mail-reply-sender.svg
    resources/icons/material/hdpi/media-album-track.svg
    resources/icons/material/hdpi/media-playback-pause.svg
    resources/icons/material/hdpi/media-playback-start.svg
    resources/icons/material/hdpi/media-playlist-shuffle.svg
    resources/icons/material/hdpi/media-record-symbolic.svg
    resources/icons/material/hdpi/media-skip-forward.svg
    resources/icons/material/hdpi/media-view-subtitles-symbolic.svg
    resources/icons/material/hdpi/multimedia-player.svg
    resources/icons/material/hdpi/overflow-menu.svg
    resources/icons/material/hdpi/package-supported.svg
    resources/icons/material/hdpi/player-volume.svg
    resources/icons/material/hdpi/settings-configure.svg
    resources/icons/material/hdpi/video.svg
    resources/icons/material/hdpi/view-fullscreen.svg
    resources/icons/material/hdpi/view-list-details.svg
    resources/icons/material/hdpi/view-list-icons.svg
    resources/icons/material/index.theme
    resources/jquery.min.js
    resources/jsoncpp.png
    resources/kde.png
    resources/localwinrt_login.html
    resources/local_login.html
    resources/mega_login.html
    resources/mpv.png
    resources/providers/4shared.png
    resources/providers/amazons3.png
    resources/providers/box.png
    resources/providers/dropbox.png
    resources/providers/google.png
    resources/providers/gphotos.png
    resources/providers/hubic.png
    resources/providers/local.png
    resources/providers/localwinrt.png
    resources/providers/mega.png
    resources/providers/onedrive.png
    resources/providers/pcloud.png
    resources/providers/webdav.png
    resources/providers/yandex.png
    resources/qt.png
    resources/style.min.css
    resources/tinyxml2.png
    resources/url.min.js
    resources/vlc-blue.png
    resources/vlc.png
    resources/webdav_login.html
    qtquickcontrols2.conf
    config.json
    ${IOS_RESOURCES}
)

set(cloudbrowser_QML
    qml/AboutView.qml
    qml/AndroidFileDialog.qml
    qml/FileDialog.qml
    qml/ImagePlayer.qml
    qml/ItemPage.qml
    qml/main.qml
    qml/MediaPlayer.qml
    qml/MpvPlayer.qml
    qml/ProviderEntry.qml
    qml/QtFileDialog.qml
    qml/QtPlayer.qml
    qml/RegisterPage.qml
    qml/RemoveProvider.qml
    qml/WebView.qml
    qml/WinRTFileDialog.qml
)

target_sources(cloudbrowser PRIVATE
    ${cloudbrowser_HEADERS}
    ${cloudbrowser_SOURCES}
    ${cloudbrowser_RESOURCES}
    ${cloudbrowser_QML}
    ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc
)

set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc PROPERTY GENERATED 1)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Header Files" FILES ${cloudbrowser_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${cloudbrowser_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Resource Files" FILES ${cloudbrowser_RESOURCES} ${cloudbrowser_QML})
source_group("Generated Files" REGULAR_EXPRESSION "((Qt5.*Import\.cpp)|(mocs_.*\.cpp)|(qrc_.*\.cpp)|(.*\.qrc))$")

foreach(ENTRY ${cloudbrowser_RESOURCES} ${cloudbrowser_QML})
    string(APPEND QRC_CONTENT "        <file alias=\"${ENTRY}\">${CMAKE_CURRENT_SOURCE_DIR}/${ENTRY}</file>\n")
endforeach()

file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc
    CONTENT
"<RCC>
    <qresource prefix=\"/\">
${QRC_CONTENT}    </qresource>
</RCC>"
)

set_target_properties(cloudbrowser
    PROPERTIES
        CXX_STANDARD 17
)

target_include_directories(cloudbrowser PRIVATE "src")

cloudstorage_target_link_library(cloudbrowser jsoncpp)

find_package(Qt5WebView CONFIG)
if(Qt5WebView_FOUND)
    target_compile_definitions(cloudbrowser PRIVATE WITH_QTWEBVIEW)
    target_link_libraries(cloudbrowser PRIVATE Qt5::WebView)
endif()

cloudstorage_find_library(mpv INCLUDE mpv/client.h NAMES mpv)
if(mpv_LIBRARY)
    target_compile_definitions(cloudbrowser PRIVATE WITH_MPV)
    cloudstorage_target_link_library(cloudbrowser mpv)
    cloudstorage_find_library(avdevice INCLUDE libavdevice/avdevice.h NAMES avdevice)
    if(avdevice_LIBRARY)
        cloudstorage_target_link_library(cloudbrowser avdevice)
    endif()
    if(APPLE)
        find_library(coreaudio CoreAudio)
        find_library(avfoundation AVFoundation)
        target_link_libraries(cloudbrowser PRIVATE ${coreaudio} ${avfoundation})
    endif()
endif()

if(ffmpeg_FOUND)
    target_compile_definitions(cloudbrowser PRIVATE WITH_THUMBNAILER)
    cloudstorage_target_link_library(cloudbrowser ffmpeg)
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
        string(REPLACE "/" "_" android_library_name "${path_suffix}/${plugin_name}")
        cloudstorage_find_library(${plugin}
            LIBRARY_PATH_SUFFIXES ../${path_suffix} ${path_suffix}
            NAMES ${plugin_name} ${android_library_name}_${CMAKE_ANDROID_ARCH_ABI}
            DEBUG_NAMES ${plugin_name}d ${android_library_name}d_${CMAKE_ANDROID_ARCH_ABI})
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
    cloudstorage_find_library(kirigami_plugin LIBRARY_PATH_SUFFIXES qml/org/kde/kirigami.2 ../qml/org/kde/kirigami.2 NAMES kirigamiplugin)

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

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(cloudbrowser PRIVATE  Qt5::QAndroidIntegrationPlugin Qt5::AndroidExtras)
    get_filename_component(QT_ROOT "${Qt5Core_DIR}/../../.." ABSOLUTE)
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/gradle.properties
                  CONTENT "
                    androidCompileSdkVersion=29
                    androidBuildToolsVersion=29.0.2
                    qt5AndroidDir=${QT_ROOT}/src/android/java
                    org.gradle.jvmargs=-Xmx4608M
                  ")
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/res/values/libs.xml
                  CONTENT "<?xml version='1.0' encoding='utf-8'?>
<resources>
    <array name=\"qt_sources\">
        <item>https://download.qt.io/ministro/android/qt5/qt-5.14</item>
    </array>
    <array name=\"bundled_libs\">
        <item>${ANDROID_ABI};cloudbrowser_${ANDROID_ABI}</item>
    </array>
    <array name=\"qt_libs\"/>
    <array name=\"load_local_libs\"/>
</resources>")
    set(cloudbrowser-android-resources
        android/AndroidManifest.xml
        android/assets/subfont.ttf
        android/build.gradle
        android/AndroidManifest.xml
        android/assets/subfont.ttf
        android/build.gradle
        android/project.properties
        android/res-common/values/style.xml
        android/res-debug/drawable/icon.png
        android/res-release/drawable/icon.png
        android/src/org/videolan/cloudbrowser/AuthView.java
        android/src/org/videolan/cloudbrowser/CloudBrowser.java
        android/src/org/videolan/cloudbrowser/FileInput.java
        android/src/org/videolan/cloudbrowser/FileOutput.java
        android/src/org/videolan/cloudbrowser/NotificationHelper.java
        android/src/org/videolan/cloudbrowser/NotificationService.java
        android/src/org/videolan/cloudbrowser/Utility.java)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/android PREFIX "Android Files" FILES ${cloudbrowser-android-resources})
    add_custom_target(cloudbrowser-apk
        ALL
        DEPENDS
            cloudbrowser
            ${cloudbrowser-android-resources}
            ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/gradle.properties
            ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/res/values/libs.xml
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/android ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/libs/${ANDROID_ABI}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cloudbrowser> ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/libs/${ANDROID_ABI}
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_ROOT}/jar/QtAndroid.jar ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/libs/
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_ROOT}/jar/QtAndroidBearer.jar ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/libs/
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_ROOT}/jar/QtAndroidExtras.jar ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk/libs/
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/cloudbrowser-apk ${QT_ROOT}/src/3rdparty/gradle/gradlew assemble
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(cloudbrowser PRIVATE Qt5::QIOSIntegrationPlugin Qt5::GuiPrivate "-e _qt_main_wrapper")
endif()

set_target_properties(cloudbrowser PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist
    RESOURCE "${IOS_RESOURCES}"
)

install(
    TARGETS cloudbrowser
    BUNDLE DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    RESOURCE DESTINATION ${CMAKE_INSTALL_PREFIX}/share/cloudbrowser/resources
)
