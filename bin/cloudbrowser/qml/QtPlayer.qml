import QtQuick 2.0
import QtMultimedia 5.6

Item {
  property alias source: player.source
  property real position: player.position / player.duration
  property bool buffering: player.status === MediaPlayer.Loading ||
                           player.status === MediaPlayer.Stalled
  property bool ended: player.status === MediaPlayer.EndOfMedia
  property int time: duration * position
  property int duration: player.duration

  signal error(int error, string errorString)

  function set_position(p) {
    player.seek(p * player.duration);
  }

  function pause() {
    player.pause();
  }

  function play() {
    player.play();
  }

  MediaPlayer {
    id: player
    autoPlay: true
    onError: container.error(error, errorString)
  }
  VideoOutput {
    source: player
    anchors.fill: parent
  }
}
