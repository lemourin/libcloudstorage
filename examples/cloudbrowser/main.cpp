/*****************************************************************************
 * main.cpp : cloudbrowser main function
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

#include <QApplication>
#include <QResizeEvent>
#include <iostream>

#include "MediaPlayer.h"
#include "Window.h"

class MainWidget : public QWidget {
 public:
  MainWidget()
      : media_player_(MediaPlayer::instance(this)),
        window_(media_player_.get()),
        container_(createWindowContainer(&window_, this)) {
    window_.set_container(container_);

    container_->setFocus();

    media_player_->setStyleSheet("background-color:black;");
    media_player_->show();
  }

 protected:
  void resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    media_player_->resize(e->size());
    container_->resize(e->size());
  }

  void keyPressEvent(QKeyEvent* e) {
    QWidget::keyPressEvent(e);
    if (e->isAccepted()) return;
    if (e->key() == Qt::Key_Q) {
      window_.stop();
      container_->setFocus();
    } else if (e->key() == Qt::Key_P)
      media_player_->pause();
  }

 private:
  std::unique_ptr<MediaPlayer> media_player_;
  Window window_;
  QWidget* container_;
};

int main(int argc, char* argv[]) {
  try {
    QApplication app(argc, argv);
    app.setOrganizationName("VideoLAN");
    app.setApplicationName("cloudbrowser");

    MainWidget widget;
    widget.resize(800, 600);
    widget.show();

    return app.exec();
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
