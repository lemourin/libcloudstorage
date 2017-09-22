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
#include <QGuiApplication>

#ifdef WITH_QTWEBVIEW
#include <QtWebView>
#endif

#include <QDebug>
#include <QIcon>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <iostream>

#include "CloudContext.h"

void register_types() {
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
  qmlRegisterUncreatableType<CloudItem>("libcloudstorage", 1, 0, "CloudItem",
                                        "uncreatable type");
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

    QQmlApplicationEngine engine(QUrl("qrc:/qml/main.qml"));

    int ret = app.exec();

    Q_CLEANUP_RESOURCE(resources);

    return ret;
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    return 1;
  }
}
