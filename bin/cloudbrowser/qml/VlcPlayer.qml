import QtQuick 2.0
import VLCQt 1.1

Item {
  property alias source: player.url
  property alias player: player
  property alias position: player.position
  property int audio_track_count: 0
  property int subtitle_track_count: 0
  property real volume: player.volume / 100.0
  property bool playing: player.state === Vlc.Playing
  property bool buffering: player.state === Vlc.Buffering || player.state === Vlc.Opening
  property bool ended: player.state === Vlc.Ended
  property int time: player.time
  property int duration: player.length

  signal error(int error, string errorString)

  function set_volume(v) {
    player.volume = v * 100;
  }

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
    autoplay: false
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

  Component.onCompleted: {
    player.volume = 100;
  }
}
