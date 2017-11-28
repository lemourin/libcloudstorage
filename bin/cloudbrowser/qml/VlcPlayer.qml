import QtQuick 2.0
import VLCQt 1.1

Item {
  property alias source: player.url
  property alias player: player
  property alias position: player.position
  property bool buffering: player.state === Vlc.Buffering || player.state === Vlc.Opening
  property bool ended: player.state === Vlc.Ended
  property int time: player.time
  property int duration: player.length

  signal error(int error, string errorString)

  function set_position(p) {
    player.position = p;
  }

  function pause() {
    player.pause();
  }

  function play() {
    player.play();
  }

  VlcPlayer {
    id: player
    autoplay: true
    onStateChanged: {
      if (state === Vlc.Error)
        error(500, "Error occurred")
    }
  }
  VlcVideoOutput {
    id: video
    source: player
    anchors.fill: parent
  }
}
