import QtQuick 2.7
import QtMultimedia 5.6
import org.kde.kirigami 2.0 as Kirigami
import libcloudstorage 1.0

Kirigami.ScrollablePage {
  property CloudItem item

  id: page
  title: "Play"

  anchors.fill: parent

  GetUrlRequest {
    id: url_request
    context: cloud
    item: page.item
  }

  MediaPlayer {
    id: mediaPlayer
    source: url_request.source
    onSourceChanged: console.log(source)
    autoPlay: true
  }
  VideoOutput {
    source: mediaPlayer
    anchors.fill: parent
    Rectangle {
      anchors.fill: parent
      color: "black"
      z: -1
    }
  }
  Rectangle {
    anchors.fill: parent
    color: "black"
  }
}
