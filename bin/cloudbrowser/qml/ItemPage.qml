import QtQuick 2.7
import org.kde.kirigami 2.1 as Kirigami
import QtQuick.Controls 2.0 as Controls
import QtQuick.Templates 2.0 as Templates
import QtQuick.Layouts 1.2
import libcloudstorage 1.0

Kirigami.ScrollablePage {
  property CloudItem item
  property string label

  id: page
  anchors.fill: parent
  supportsRefreshing: true

  onIsCurrentPageChanged: {
    if (isCurrentPage)
      root.selected_item = list_view.currentItem ? list_view.currentItem.item : null;
  }

  onRefreshingChanged: {
    if (refreshing) {
      list.update(cloud, item);
      refreshing = false;
    }
  }

  onBackRequested: {
    if (pageStack.currentIndex > 0) {
      pageStack.currentIndex--;
      event.accepted = true;
    }
  }

  function playable_type(type) {
    return type === "audio" || type === "video" || type === "image";
  }

  function nextRequested() {
    while (list_view.currentIndex + 1 < list_view.count) {
      list_view.currentIndex++;
      if (playable_type(list_view.currentItem.item.type))
        return list_view.currentItem.item;
    }
    return null;
  }

  Component {
    id: media_player
    Kirigami.Page {
      property CloudItem item
      property var item_page
      property bool handle_state

      id: player_page
      title: item.filename
      leftPadding: 0
      rightPadding: 0
      topPadding: 0
      bottomPadding: 0

      onItemChanged: {
        url_request.update(cloud, item);
        update_notification();
      }

      onBackRequested: {
        if (pageStack.currentIndex > 0) {
          pageStack.currentIndex--;
          event.accepted = true;
        }
      }

      onIsCurrentPageChanged: {
        if (!isCurrentPage) {
          platform.disableKeepScreenOn();
          root.fullscreen_player = false;
          root.visible_player = false;
          root.globalDrawer.handleVisible = handle_state;
        } else {
          handle_state = root.globalDrawer.handleVisible;
          root.globalDrawer.handleVisible = false;
          root.visible_player = true;
          platform.enableKeepScreenOn();
        }
      }

      GetUrlRequest {
        id: url_request
        onSourceChanged: player.url = source
        onDoneChanged: {
          if (done && source === "") {
            if (!player.autoplay) {
              root.pageStack.pop();
            } else {
              player.next();
            }
          }
        }
      }

      function update_notification() {
        if (item.type === "audio" || item.type === "video")
          platform.showPlayerNotification(player.playing, item.filename, item_page ? item_page.label : "");
        else
          platform.hidePlayerNotification();
      }

      MediaPlayer {
        anchors.fill: parent
        id: player
        player: cloud.playerBackend
        type: item.type
        volume: root.volume

        Connections {
          target: platform
          onNotify: {
            if (action === "PLAY")
              playing = true;
            else if (action === "PAUSE")
              playing = false;
            else if (action === "NEXT") {
              autoplay = true;
              next();
            }
          }
        }

        onEnded: {
          if (!autoplay) {
            root.pageStack.pop();
          }
        }

        onNext: {
          var next = item_page.nextRequested();
          if (next)
            item = next;
          else
            root.pageStack.pop();
        }

        onPlayingChanged: {
          update_notification();
        }

        onErrorOccurred: {
          cloud.errorOccurred(domain, null, 500, message);
        }

        onFullscreenChanged: root.fullscreen_player = fullscreen;

        Component.onCompleted: {
          root.player_count++;
          update_notification();
        }
        Component.onDestruction: {
          root.player_count--;
          if (root.player_count === 0)
             platform.hidePlayerNotification();
        }
      }
    }
  }

  contextualActions: [
    Kirigami.Action {
      id: play_action
      visible: list_view.currentItem && playable_type(list_view.currentItem.item.type)
      text: "Play"
      iconName: "media-playback-start"
      onTriggered: {
        contextDrawer.drawerOpen = false;
        pageStack.push(media_player, {item: list_view.currentItem.item, item_page: page});
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem && detailed_options && item.supports("rename")
      text: "Rename"
      iconName: "edit-cut"
      onTriggered: {
        list_view.currentEdit = list_view.currentIndex;
      }
    },
    Kirigami.Action {
      visible: detailed_options && item.supports("mkdir")
      iconName: "folder-new"
      text: "Create Directory"
      onTriggered: {
        list_view.currentIndex = -1;
        create_directory_sheet.open();
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem && detailed_options && item.supports("delete")
      text: "Delete"
      iconName: "edit-delete"
      onTriggered: {
        delete_request.update(cloud, list_view.currentItem.item);
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem && !cloud.currently_moved && detailed_options && item.supports("move")
      text: "Move"
      iconName: "folder-move"
      onTriggered: {
        cloud.list_request = list;
        cloud.currently_moved = list_view.currentItem.item;
      }
    },
    Kirigami.Action {
      visible: cloud.currently_moved && detailed_options && item.supports("move")
      text: "Move " + (cloud.currently_moved ? cloud.currently_moved.filename : "") + " here"
      iconName: "dialog-apply"
      onTriggered: {
        move_request.update(cloud, cloud.currently_moved, page.item);
        if (cloud.list_request)
          cloud.list_request.done = false;
        cloud.currently_moved = null;
      }
    },
    Kirigami.Action {
      visible: cloud.currently_moved && detailed_options && item.supports("move")
      text: "Cancel move"
      iconName: "dialog-cancel"
      onTriggered: {
        cloud.currently_moved = null;
        cloud.list_request = null;
      }
    },
    Kirigami.Action {
      visible: item.supports("upload")
      text: "Upload File"
      iconName: "edit-add"
      onTriggered: upload_dialog.open()
    },
    Kirigami.Action {
      visible: list_view.currentItem && list_view.currentItem.item.type !== "directory" &&
               item.supports("download")
      iconName: "document-save"
      text: "Download"
      onTriggered: {
        download_dialog.filename = list_view.currentItem.item.filename;
        download_dialog.open();
      }
    }
  ]

  function show_size(size) {
    if (size < 1024)
      return size + " B";
    else if (size < 1024 * 1024)
      return (size / 1024).toFixed(2) + " KB";
    else if (size < 1024 * 1024 * 1024)
      return (size / (1024 * 1024)).toFixed(2) + " MB";
    else
      return (size / (1024 * 1024 * 1024)).toFixed(2) + " GB";
  }

  Kirigami.OverlaySheet {
    id: create_directory_sheet
    ColumnLayout {
      anchors.centerIn: parent
      Controls.TextField {
        id: directory_name
        color: Kirigami.Theme.textColor
        placeholderText: "New Directory"
      }
      Controls.Button {
        CreateDirectoryRequest {
          id: create_directory
          onCreatedDirectory: refreshing = true
        }
        Layout.alignment: Qt.AlignHCenter
        text: "Create"
        onClicked: {
          create_directory.update(cloud, item, directory_name.text);
          create_directory_sheet.close();
        }
      }
    }
  }

  Controls.BusyIndicator {
    anchors.centerIn: parent
    enabled: false
    running: !list.done && list_view.count === 0
    width: 200
    height: 200
  }

  ListView {
    property int currentEdit: -1

    ListDirectoryRequest {
      property CloudItem item: page.item

      id: list
      Component.onCompleted: update(cloud, item)
    }

    DeleteItemRequest {
      id: delete_request
      onItemDeleted: refreshing = true
    }

    RenameItemRequest {
      id: rename_request
      onItemRenamed: refreshing = true
    }

    MoveItemRequest {
      id: move_request
      onItemMoved: {
        if (cloud.list_request)
          cloud.list_request.update(cloud, cloud.list_request.item);
        cloud.list_request = null;
        refreshing = true;
      }
    }

    FileDialog {
      id: upload_dialog
      selectExisting: true
      onAccepted: {
        var r = upload_component.createObject(cloud, {
                                                filename: upload_dialog.filename,
                                                list: list
                                              });
        r.update(cloud, page.item, fileUrl, upload_dialog.filename);
        if (r.done === false) {
          var req = cloud.request;
          req.push(r);
          cloud.request = req;
        }
      }
    }

    FileDialog {
      id: download_dialog
      selectExisting: false
      onAccepted: {
        var r = download_component.createObject(cloud, {
                                                  filename: download_dialog.filename
                                                });
        r.update(cloud, list_view.currentItem.item, fileUrl);
        if (r.done === false) {
          var req = cloud.request;
          req.push(r);
          cloud.request = req;
        }
      }
    }

    id: list_view
    model: list.list
    currentIndex: -1
    onCurrentItemChanged: root.selected_item = currentItem ? currentItem.item : null
    onCurrentIndexChanged: currentEdit = -1
    footerPositioning: ListView.OverlayFooter
    footer: Kirigami.ItemViewHeader {
      title: ""
      visible: cloud.request.length > 0
      width: parent.width
      height: 150
      ListView {
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        model: cloud.request
        delegate: RowLayout {
          width: parent.width
          height: 40
          Item {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            height: parent.height
            Templates.Label {
              anchors.fill: parent
              anchors.margins: 10
              text: modelData.filename
              elide: Text.ElideRight
            }
          }
          Kirigami.Icon {
            id: icon
            Layout.alignment: Qt.AlignRight
            Layout.margins: 10
            width: 20
            height: 20
            source: modelData.upload ? "go-up" : "go-down"
          }
          Controls.ProgressBar {
            id: progress
            Layout.alignment: Qt.AlignRight
            Layout.margins: 10
            from: 0
            to: 1
            value: modelData.progress
          }
        }
      }
    }
    delegate: Kirigami.BasicListItem {
      property var item: modelData
      property alias text_control: text

      id: item
      backgroundColor: (ListView.isCurrentItem && list_view.currentEdit === -1) ?
                         Kirigami.Theme.highlightColor : Kirigami.Theme.backgroundColor
      reserveSpaceForIcon: false
      height: 80
      onClicked: {
        if (list_view.currentEdit == index) return;
        list_view.currentIndex = index;
        if (modelData.type === "directory")
          cloud.list(modelData.filename, page.label, modelData);
        else
          root.contextDrawer.drawerOpen = true;
      }
      RowLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignHCenter
        GetThumbnailRequest {
          id: thumbnail
          Component.onCompleted: update(cloud, modelData)
        }
        Item {
          id: image
          width: 64
          height: 64
          Layout.rightMargin: 10
          Controls.BusyIndicator {
            anchors.fill: parent
            running: !item_icon.visible && !item_thumbnail.visible
          }
          Kirigami.Icon {
            function type_to_icon() {
              if (modelData.type === "directory")
                return "folder";
              else if (modelData.type === "image")
                return "image";
              else if (modelData.type === "video")
                return "video";
              else if (modelData.type === "audio")
                return "audio-x-generic";
              else
                return "gtk-file";
            }

            id: item_icon
            anchors.fill: parent
            anchors.margins: 5
            visible: thumbnail.done && (modelData.type === "directory" || !thumbnail.source)
            source: type_to_icon(modelData.type)
          }
          Image {
            id: item_thumbnail
            anchors.fill: parent
            asynchronous: true
            mipmap: true
            fillMode: Image.PreserveAspectCrop
            anchors.margins: 5
            visible: thumbnail.done && thumbnail.source && !item_icon.visible
            source: thumbnail.source
          }
          MultiPointTouchArea {
            anchors.fill: parent
            onPressed: list_view.currentIndex = index;
          }
        }
        Item {
          Layout.fillWidth: true
          Layout.fillHeight: true
          visible: list_view.currentEdit !== index
          RowLayout {
            anchors.fill: parent
            ColumnLayout {
              id: description
              Layout.fillWidth: true
              Templates.Label {
                id: filename
                text: modelData.filename
                Layout.fillWidth: true
                font.pointSize: 16
                color: Kirigami.Theme.textColor
                elide: Text.ElideRight
              }
              Templates.Label {
                text: modelData.timestamp
                color: Kirigami.Theme.disabledTextColor
              }
            }
            Templates.Label {
              Layout.alignment: Qt.AlignRight | Qt.AlignTop
              text: modelData.size !== -1 ? show_size(modelData.size) : ""
              color: Kirigami.Theme.disabledTextColor
            }
          }
        }
        Item {
          Layout.fillWidth: true
          Layout.fillHeight: true
          visible: list_view.currentEdit === index
          RowLayout {
            anchors.fill: parent
            onVisibleChanged: if (visible && modelData) text.text = modelData.filename
            Controls.TextField {
              id: text
              Layout.fillWidth: true
              color: Kirigami.Theme.textColor
            }
            Controls.Button {
              id: rename_button
              Layout.alignment: Qt.AlignRight
              Layout.leftMargin: 8
              text: "Rename"
              onClicked: {
                list_view.currentEdit = -1;
                rename_request.update(cloud, modelData, text.text);
              }
            }
          }
        }
      }
    }
  }
}
