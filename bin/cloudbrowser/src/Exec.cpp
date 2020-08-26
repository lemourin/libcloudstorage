/*****************************************************************************
 * Exec.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "Exec.h"
#include <QGuiApplication>

#ifdef WITH_QTWEBVIEW
#include <QtWebView>
#endif

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>

#include "AndroidUtility.h"
#include "CloudContext.h"
#include "FileDialog.h"
#include "IPlatformUtility.h"
#include "MpvPlayer.h"
#include "Request/CreateDirectory.h"
#include "Request/DeleteItem.h"
#include "Request/DownloadItem.h"
#include "Request/GetThumbnail.h"
#include "Request/GetUrl.h"
#include "Request/ListDirectory.h"
#include "Request/MoveItem.h"
#include "Request/RenameItem.h"
#include "Request/UploadItem.h"
#include "Utility/Utility.h"
#include "WinRTUtility.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#include <qpa/qplatformwindow.h>
#endif
#endif

#ifdef QT_STATICPLUGIN

#include <QtPlugin>

#define KIRIGAMI_BUILD_TYPE_STATIC
#include "kirigamiplugin.h"

Q_IMPORT_PLUGIN(QtQuick2Plugin)
Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)
Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
Q_IMPORT_PLUGIN(QtQuickControls2MaterialStylePlugin)
Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
Q_IMPORT_PLUGIN(QtQmlPlugin)
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QtLabsPlatformPlugin)
Q_IMPORT_PLUGIN(QtGraphicalEffectsPlugin)
Q_IMPORT_PLUGIN(QtGraphicalEffectsPrivatePlugin)
Q_IMPORT_PLUGIN(KirigamiPlugin)

#ifdef WITH_QTWEBVIEW
Q_IMPORT_PLUGIN(QWebViewModule)
#endif

#endif

void register_types() {
  qRegisterMetaType<cloudstorage::IItem::Pointer>();
  qRegisterMetaType<
      cloudstorage::EitherError<std::vector<cloudstorage::IItem::Pointer>>>();
  qRegisterMetaType<cloudstorage::EitherError<void>>();
  qRegisterMetaType<cloudstorage::EitherError<std::string>>();
  qRegisterMetaType<cloudstorage::EitherError<cloudstorage::IItem>>();
  qRegisterMetaType<cloudstorage::EitherError<QVariant>>();
  qRegisterMetaType<ProviderListModel*>();
  qRegisterMetaType<ListDirectoryModel*>();
  qmlRegisterType<CloudContext>("libcloudstorage", 1, 0, "CloudContext");
  qmlRegisterType<ListDirectoryRequest>("libcloudstorage", 1, 0,
                                        "ListDirectoryRequest");
  qmlRegisterType<GetThumbnailRequest>("libcloudstorage", 1, 0,
                                       "GetThumbnailRequest");
  qmlRegisterType<GetUrlRequest>("libcloudstorage", 1, 0, "GetUrlRequest");
  qmlRegisterType<CreateDirectoryRequest>("libcloudstorage", 1, 0,
                                          "CreateDirectoryRequest");
  qmlRegisterType<DeleteItemRequest>("libcloudstorage", 1, 0,
                                     "DeleteItemRequest");
  qmlRegisterType<RenameItemRequest>("libcloudstorage", 1, 0,
                                     "RenameItemRequest");
  qmlRegisterType<MoveItemRequest>("libcloudstorage", 1, 0, "MoveItemRequest");
  qmlRegisterType<UploadItemRequest>("libcloudstorage", 1, 0,
                                     "UploadItemRequest");
  qmlRegisterType<DownloadItemRequest>("libcloudstorage", 1, 0,
                                       "DownloadItemRequest");
#ifdef __ANDROID__
  qmlRegisterType<FileDialog>("libcloudstorage", 1, 0, "AndroidFileDialog");
#endif
#ifdef WINRT
  qmlRegisterType<FileDialog>("libcloudstorage", 1, 0, "WinRTFileDialog");
#endif
  qmlRegisterUncreatableType<CloudItem>("libcloudstorage", 1, 0, "CloudItem",
                                        "uncreatable type");
#ifdef WITH_MPV
  qmlRegisterType<MpvPlayer>("libcloudstorage", 1, 0, "MpvPlayer");
#endif

#ifdef QT_STATICPLUGIN
#ifdef WITH_QTWEBVIEW
#ifndef TARGET_OS_IPHONE
  qobject_cast<QQmlExtensionPlugin*>(
      qt_static_plugin_QWebViewModule().instance())
      ->registerTypes("QtWebView");
#endif
#endif
#ifdef KIRIGAMI_BUILD_TYPE_STATIC
  KirigamiPlugin::getInstance().registerTypes();
#endif
#endif
}

int exec_cloudbrowser(int argc, char** argv) {
  try {
    Q_INIT_RESOURCE(resources);

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);

#ifdef WITH_QTWEBVIEW
    QtWebView::initialize();
#endif

    if (!qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP"))
      qputenv("XDG_CURRENT_DESKTOP", "1");

    app.setOrganizationName("VideoLAN");
    app.setApplicationName("cloudbrowser");
    app.setWindowIcon(QPixmap(":/resources/cloud.png"));

    QIcon::setThemeSearchPaths({":/resources/icons"});
    QIcon::setThemeName("material");

    register_types();

    QSettings::setDefaultFormat(QSettings::IniFormat);

    IPlatformUtility::Pointer platform = IPlatformUtility::create();
    QQmlApplicationEngine engine;

#ifdef WITH_QTWEBVIEW
    engine.rootContext()->setContextProperty("qtwebview", QVariant(true));
#else
    engine.rootContext()->setContextProperty("qtwebview", QVariant(false));
#endif
#ifdef WITH_MPV
    engine.rootContext()->setContextProperty("mpv", QVariant(true));
#else
    engine.rootContext()->setContextProperty("mpv", QVariant(false));
#endif
    engine.rootContext()->setContextProperty("platform", platform.get());
    engine.rootContext()->setContextProperty("seperator", QDir::separator());
#ifdef WITH_THUMBNAILER
    engine.rootContext()->setContextProperty("thumbnailer", QVariant(true));
    engine.addImageProvider("thumbnailer", new ThumbnailGenerator);
#else
    engine.rootContext()->setContextProperty("thumbnailer", QVariant(false));
#endif
    engine.load(QUrl("qrc:/qml/main.qml"));

    setlocale(LC_NUMERIC, "C");
    int ret = app.exec();

    Q_CLEANUP_RESOURCE(resources);

    return ret;
  } catch (const std::exception& e) {
    cloudstorage::util::log("Exception:", e.what());
    return 1;
  }
}
