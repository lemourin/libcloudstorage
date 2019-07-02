/*****************************************************************************
 * IPlatformUtility.h
 *
 *****************************************************************************
 * Copyright (C) 2018 CodiLime
 *
 * Authors: Paweł Wegner <pawel.wegner@codilime.com>
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
#ifndef IPLATFORMUTILITY_H
#define IPLATFORMUTILITY_H

#include <QObject>
#include <QSettings>
#include <memory>

#include "Exec.h"

class QWindow;

class CLOUDBROWSER_API IPlatformUtility : public QObject {
 public:
  using Pointer = std::unique_ptr<IPlatformUtility>;

  static Pointer create();

  Q_INVOKABLE virtual void initialize(QWindow* window) const = 0;
  Q_INVOKABLE virtual QString name() const = 0;
  Q_INVOKABLE virtual bool mobile() const = 0;
  Q_INVOKABLE virtual bool openWebPage(QString url) = 0;
  Q_INVOKABLE virtual void closeWebPage() = 0;
  Q_INVOKABLE virtual void landscapeOrientation() = 0;
  Q_INVOKABLE virtual void defaultOrientation() = 0;
  Q_INVOKABLE virtual void showPlayerNotification(bool playing,
                                                  QString filename,
                                                  QString title) = 0;
  Q_INVOKABLE virtual void enableKeepScreenOn() = 0;
  Q_INVOKABLE virtual void disableKeepScreenOn() = 0;
  Q_INVOKABLE virtual void hidePlayerNotification() = 0;
  Q_INVOKABLE virtual void showAd() = 0;
  Q_INVOKABLE virtual void hideAd() = 0;

  Q_INVOKABLE qreal volume() const {
    return settings_.value("volume", 1).toReal();
  }

  Q_INVOKABLE void saveVolume(qreal value) {
    settings_.setValue("volume", value);
  }

 signals:
  void notify(QString action);

 private:
  QSettings settings_;

  Q_OBJECT
};

#endif  // IPLATFORMUTILITY_H
