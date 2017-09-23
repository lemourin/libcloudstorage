import QtQuick 2.7
import org.kde.kirigami 2.0 as Kirigami
import libcloudstorage 1.0

Kirigami.Page {
  property CloudItem item

  id: page
  title: "Play"

  anchors.fill: parent

  GetUrlRequest {
    id: url_request
    context: cloud
    item: page.item
  }

  Component {
    id: vlc_player
    VlcPlayer {
      anchors.fill: parent
      source: url_request.source
    }
  }

  Component {
    id: qt_player
    QtPlayer {
      anchors.fill: parent
      source: url_request.source
    }
  }

  Loader {
    anchors.fill: parent
    sourceComponent: vlcqt ? vlc_player : qt_player
  }
}
