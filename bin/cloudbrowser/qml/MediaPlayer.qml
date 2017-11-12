import QtQuick 2.7
import QtQuick.Controls 2.0 as Controls
import org.kde.kirigami 2.0 as Kirigami
import libcloudstorage 1.0

Kirigami.Page {
  property CloudItem item
  property bool handle_state

  id: page
  leftPadding: 0
  rightPadding: 0
  topPadding: 0
  bottomPadding: 0
  title: item.filename
  onIsCurrentPageChanged: {
    if (!isCurrentPage) {
      root.visible_player = false;
      root.globalDrawer.handleVisible = handle_state;
    } else {
      handle_state = root.globalDrawer.handleVisible;
      root.globalDrawer.handleVisible = false;
    }
  }

  GetUrlRequest {
    id: url_request
    onSourceChanged: if (player.item) player.item.source = source
    Component.onCompleted: update(cloud, page.item)
  }

  MouseArea {
    property bool playing: true

    anchors.fill: parent

    Rectangle {
      anchors.fill: parent
      color: "black"
    }

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
  Row {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: 50
    MouseArea {
      anchors.margins: 10
      anchors.verticalCenter: parent.verticalCenter
      height: parent.height
      width: parent.width - fullscreen.width
      Controls.ProgressBar {
        opacity: 0.3
        anchors.fill: parent
        from: 0
        to: 1
        value: player.item.position
      }
      onClicked: {
        player.item.set_position(mouse.x / width);
      }
    }
    Kirigami.Icon {
      id: fullscreen
      anchors.margins: 10
      width: height
      height: parent.height
      source: "view-fullscreen"

      MouseArea {
        anchors.fill: parent
        onClicked: root.visible_player ^= 1
      }
    }
  }
}
