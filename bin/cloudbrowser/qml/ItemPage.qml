import QtQuick 2.7
import org.kde.kirigami 2.1 as Kirigami
import QtQuick.Controls 2.0 as Controls
import QtQuick.Templates 2.0 as Templates
import QtQuick.Layouts 1.2
import libcloudstorage 1.0

Kirigami.ScrollablePage {
  property CloudItem item

  id: page
  anchors.fill: parent
  supportsRefreshing: true

  onRefreshingChanged: {
    if (refreshing) {
      list.update(cloud, item);
      refreshing = false;
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
    MediaPlayer {
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
      visible: list_view.currentItem && detailed_options
      text: "Rename"
      iconName: "edit-cut"
      onTriggered: {
        list_view.currentEdit = list_view.currentIndex;
      }
    },
    Kirigami.Action {
      visible: detailed_options
      iconName: "folder-new"
      text: "Create Directory"
      onTriggered: {
        list_view.currentIndex = -1;
        create_directory_sheet.open();
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem && detailed_options
      text: "Delete"
      iconName: "edit-delete"
      onTriggered: {
        delete_request.update(cloud, list_view.currentItem.item);
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem && !cloud.currently_moved && detailed_options
      text: "Move"
      iconName: "folder-move"
      onTriggered: {
        cloud.list_request = list;
        cloud.currently_moved = list_view.currentItem.item;
      }
    },
    Kirigami.Action {
      visible: cloud.currently_moved && detailed_options
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
      visible: cloud.currently_moved && detailed_options
      text: "Cancel move"
      iconName: "dialog-cancel"
      onTriggered: {
        cloud.currently_moved = null;
        cloud.list_request = null;
      }
    },
    Kirigami.Action {
      text: "Upload File"
      iconName: "edit-add"
      onTriggered: upload_dialog.open()
    },
    Kirigami.Action {
      visible: list_view.currentItem && list_view.currentItem.item.type !== "directory"
      iconName: "document-save"
      text: "Download " + (list_view.currentItem ? list_view.currentItem.item.filename : "")
      onTriggered: {
        download_dialog.filename = list_view.currentItem.item.filename;
        download_dialog.open();
      }
    }
  ]

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

        anchors.horizontalCenter: parent.horizontalCenter
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
    running: !list.done
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
      selectFolder: false
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
      selectFolder: true
      selectExisting: true
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
            anchors.left: parent.left
            anchors.right: icon.left
            height: parent.height
            clip: true
            Templates.Label {
              anchors.fill: parent
              anchors.margins: 10
              text: modelData.filename
            }
          }
          Kirigami.Icon {
            id: icon
            anchors.right: progress.left
            anchors.margins: 10
            width: 20
            height: 20
            source: modelData.upload ? "go-up" : "go-down"
          }
          Controls.ProgressBar {
            id: progress
            anchors.right: parent.right
            anchors.margins: 10
            from: 0
            to: modelData.total
            value: modelData.now
          }
        }
      }
    }
    delegate: Kirigami.BasicListItem {
      property var item: modelData
      property alias text_control: text

      id: item
      backgroundColor: ListView.isCurrentItem ? Kirigami.Theme.highlightColor :
                                                Kirigami.Theme.backgroundColor
      onClicked: {
        list_view.currentIndex = index;
        if (modelData.type === "directory")
          cloud.list(modelData.filename, modelData);
        else
          root.contextDrawer.drawerOpen = true;
      }
      Row {
        anchors.left: parent.left
        anchors.right: parent.right
        GetThumbnailRequest {
          id: thumbnail
          Component.onCompleted: update(cloud, modelData)
        }
        Item {
          id: image
          width: 50
          height: 50
          Controls.BusyIndicator {
            anchors.fill: parent
            running: !thumbnail.done
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
            anchors.fill: parent
            anchors.margins: 5
            visible: thumbnail.done && thumbnail.source && !item_icon.visible
            source: thumbnail.source
          }
          MultiPointTouchArea {
            anchors.fill: parent
            onPressed: list_view.currentIndex = index;
          }
        }
        Templates.Label {
          anchors.verticalCenter: parent.verticalCenter
          text: modelData.filename
          color: Kirigami.Theme.textColor
          visible: list_view.currentEdit !== index
        }
        RowLayout {
          visible: list_view.currentEdit === index
          onVisibleChanged: if (visible && modelData) text.text = modelData.filename
          width: parent.width - image.width
          height: parent.height
          Controls.TextField {
            id: text
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 10
            anchors.left: parent.left
            anchors.right: rename_button.left
            Layout.alignment: Qt.AlignLeft
            color: Kirigami.Theme.textColor
          }
          Controls.Button {
            id: rename_button
            anchors.margins: 10
            Layout.alignment: Qt.AlignRight
            anchors.right: parent.right
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
