import QtQuick 2.7
import QtQuick.Controls 2.0 as Controls
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
    onSourceChanged: if (player.item) player.item.source = source
  }

  MouseArea {
    property bool playing: true

    anchors.fill: parent
    Loader {
      id: player
      anchors.fill: parent
      asynchronous: true
      source: vlcqt ? "VlcPlayer.qml" : "QtPlayer.qml"
      onStatusChanged: if (status === Loader.Ready) item.source = url_request.source;
    }
    onClicked: {
      playing ^= 1;
      if (playing)
        player.item.play();
      else
        player.item.pause();
    }
  }
  MouseArea {
    height: 20
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    Controls.ProgressBar {
      anchors.fill: parent
      from: 0
      to: 1
      value: player.item.position
    }
    onClicked: {
      player.item.set_position(mouse.x / width);
    }
  }
}
