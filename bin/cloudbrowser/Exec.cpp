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

#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>

#ifdef __ANDROID__
#include "AndroidUtility.h"
#include "FileDialog.h"
#endif
#include "CloudContext.h"
#include "Utility/Utility.h"

#ifdef WITH_VLC_QT
#include <VLCQtQml/Qml.h>
#endif

void register_types() {
  qRegisterMetaType<cloudstorage::IItem::Pointer>();
  qRegisterMetaType<
      cloudstorage::EitherError<std::vector<cloudstorage::IItem::Pointer>>>();
  qRegisterMetaType<cloudstorage::EitherError<void>>();
  qRegisterMetaType<cloudstorage::EitherError<std::string>>();
  qRegisterMetaType<cloudstorage::EitherError<cloudstorage::IItem>>();
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
  qmlRegisterUncreatableType<CloudItem>("libcloudstorage", 1, 0, "CloudItem",
                                        "uncreatable type");
#ifdef WITH_VLC_QT
  VlcQml::registerTypes();
#endif
}

int exec_cloudbrowser(int argc, char** argv) {
  try {
    Q_INIT_RESOURCE(resources);
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

#ifdef WITH_QTWEBVIEW
    QtWebView::initialize();
#endif

    app.setOrganizationName("VideoLAN");
    app.setApplicationName("cloudbrowser");
    app.setWindowIcon(QPixmap(":/resources/cloud.png"));

    register_types();

    QQmlApplicationEngine engine;
#ifdef WITH_QTWEBVIEW
    engine.rootContext()->setContextProperty("qtwebview", QVariant(true));
#else
    engine.rootContext()->setContextProperty("qtwebview", QVariant(false));
#endif
#ifdef WITH_VLC_QT
    engine.rootContext()->setContextProperty("vlcqt", QVariant(true));
#else
    engine.rootContext()->setContextProperty("vlcqt", QVariant(false));
#endif
#ifdef __ANDROID__
    AndroidUtility android_utility;
    engine.rootContext()->setContextProperty("android", &android_utility);
#else
    engine.rootContext()->setContextProperty("android", QVariant(false));
#endif

    engine.rootContext()->setContextProperty("seperator", QDir::separator());
    engine.load(QUrl("qrc:/qml/main.qml"));

    int ret = app.exec();

    Q_CLEANUP_RESOURCE(resources);

    return ret;
  } catch (const std::exception& e) {
    cloudstorage::util::log("Exception: ", e.what());
    return 1;
  }
}
