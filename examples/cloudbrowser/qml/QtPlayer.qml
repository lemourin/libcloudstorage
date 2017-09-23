import QtQuick 2.0
import QtMultimedia 5.6

MouseArea {
  property alias source: mediaPlayer.source

  MediaPlayer {
    id: mediaPlayer
    autoPlay: true
  }
  VideoOutput {
    source: mediaPlayer
    anchors.fill: parent
  }
}
