import QtQuick 2.0
import VLCQt 1.1

Item {
  property alias source: player.url
  property alias player: player
  property alias position: player.position
  property var subtitle_tracks: []
  property var subtitle_id: []
  property int audio_track_count: player.audioTrackModel.count - 1
  property int subtitle_track_count: player.subtitleTrackModel.count - 1
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

  Connections {
    property int title_id: 256 + 2
    property int identifier_id: 256 + 1
    target: player.subtitleTrackModel
    onRowsRemoved: {
      subtitle_tracks = [];
      subtitle_id = [];
    }
    onRowsInserted: {
      var id = target.data(target.index(first, 0), identifier_id);
      subtitle_id.push(id);
      var title = target.data(target.index(first, 0), title_id)
      subtitle_tracks.push(title);
      subtitle_tracks = subtitle_tracks;
      if (first === 0) {
        set_subtitle_track(0);
      }
    }
  }

  Component.onCompleted: {
    player.volume = 100;
  }

  function set_subtitle_track(track) {
    player.subtitleTrack = subtitle_id[track];
  }
}
