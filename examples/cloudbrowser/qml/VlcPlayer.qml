import QtQuick 2.0
import VLCQt 1.1

MouseArea {
  property alias source: player.url

  VlcPlayer {
    id: player
  }
  VlcVideoOutput {
    id: video
    source: player
    anchors.fill: parent
  }
}
