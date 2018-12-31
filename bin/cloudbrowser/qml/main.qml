import QtQuick 2.7
import QtQuick.Controls 2.0 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.0 as Kirigami
import libcloudstorage 1.0

Kirigami.ApplicationWindow {
  property bool include_ads: cloud.includeAds
  property bool ad_loaded: false
  property bool ad_visible: include_ads
  property bool visible_player: false
  property bool fullscreen_player: false
  property bool drawer_state: false
  property bool detailed_options: !platform.mobile() || root.height > root.width
  property bool auth_error_occurred: false
  property real volume: 1
  property real last_volume: 1
  property int player_count: 0
  property int footer_height: ad_loaded && ad_visible ? 50 : 0
  property CloudItem selected_item

  id: root
  width: Math.min(800, screen.desktopAvailableWidth)
  height: Math.min(600, screen.desktopAvailableHeight)

  onFullscreen_playerChanged: {
    if (fullscreen_player) {
      drawer_state = globalDrawer.drawerOpen;
      globalDrawer.drawerOpen = false;
      if (platform.mobile()) platform.landscapeOrientation();
      if (include_ads) {
        platform.hideAd();
        ad_visible = false;
      }
      root.showFullScreen();
    } else {
      globalDrawer.drawerOpen = drawer_state;
      if (platform.mobile()) platform.defaultOrientation();
      if (include_ads) {
        platform.showAd();
        ad_visible = true;
      }
      root.show();
    }
  }

  header: Kirigami.ApplicationHeader {
    id: header
    width: root.width
    visible: !fullscreen_player
  }

  Connections {
    target: platform
    onNotify: {
      if (action === "HIDE_AD")
        ad_loaded = false;
      else if (action === "SHOW_AD")
        ad_loaded = true;
    }
  }

  Connections {
    target: cloud.userProviders
    onUpdated: {
      root.globalDrawer.actions = root.actions(cloud.userProviders.variant());
    }
  }

  CloudContext {
    property var list_request
    property var currently_moved
    property variant request: []

    id: cloud
    onErrorOccurred: {
      if (operation !== "GetThumbnail") {
        var message = "";
        if (provider.label)
          message += "(" + cloud.pretty(provider.type) + ", " + provider.label + ") ";
        message +=
            operation + ": " + code +
            (description ? " " + description : "")
        if (code === 401) {
          auth_error_occurred = true;
          message = "Revoked credentials, please reauthenticate.";
          pageStack.clear();
          cloud.removeProvider(provider);
          pageStack.push(addProviderPage);
          pageStack.currentItem.open_auth(cloud.authorizationUrl(provider.type));
        }
        root.showPassiveNotification(message);
      }
    }

    function list(title, label, item) {
      auth_error_occurred = false;
      var obj = listDirectoryPage.createObject(null, {title: title, item: item, label: label});
      if (!auth_error_occurred)
        pageStack.push(obj);
    }
  }

  function elided(str, length) {
    if (str.length <= length)
      return str;
    else
      return str.substr(0, length) + "...";
  }

  function actions(userProviders) {
    var ret = [settings.createObject(root.globalDrawer)], i;
    if (cloud.isFree) {
      ret.push(supportAction.createObject(root.globalDrawer));
    }
    for (i = 0; i < userProviders.length; i++) {
      var props = {
        provider: userProviders[i],
        iconName: "qrc:/resources/providers/" + userProviders[i].type + ".png"
      };
      ret.push(providerAction.createObject(root.globalDrawer, props));
    }
    return ret;
  }

  function bold_text(flag, text) {
    return flag ? ("<b>" + text + "</b>") : text;
  }

  globalDrawer: Kirigami.GlobalDrawer {
    Component {
      id: settings
      Kirigami.Action {
        text: "Settings"
        iconName: "settings-configure"
        Kirigami.Action {
          text: "Add Cloud Provider"
          iconName: "edit-add"
          onTriggered: {
            pageStack.clear();
            pageStack.push(addProviderPage)
          }
        }
        Kirigami.Action {
          text: "Remove Cloud Provider"
          iconName: "edit-delete"
          onTriggered: {
            pageStack.clear();
            pageStack.push(removeProviderPage);
          }
        }
        Kirigami.Action {
          text: "Select media player"
          iconName: "multimedia-player"
          visible: mpv && vlcqt
          Kirigami.Action {
            iconName: "qrc:/resources/vlc.png"
            text: bold_text(cloud.playerBackend === "vlc", "VLC")
            onTriggered: {
              cloud.playerBackend = "vlc";
            }
          }
          Kirigami.Action {
            iconName: "qrc:/resources/mpv.png"
            text: bold_text(cloud.playerBackend === "mpv", "MPV")
            onTriggered: {
              cloud.playerBackend = "mpv";
            }
          }
          Kirigami.Action {
            iconName: "qrc:/resources/qt.png"
            text: bold_text(cloud.playerBackend === "qt", "Qt")
            onTriggered: {
              cloud.playerBackend = "qt";
            }
          }
        }
        Kirigami.Action {
          text: "Clear cache (" + (cloud.cacheSize / 1048576).toFixed(2) + " MB)"
          iconName: "edit-paste-style"
          onTriggered: {
            cloud.clearCache();
          }
        }
        Kirigami.Action {
          text: "About"
          iconName: "help-about"
          onTriggered: {
            pageStack.clear();
            pageStack.push(aboutView);
          }
        }
      }
    }

    Component {
      id: supportAction
      Kirigami.Action {
        text: "Support this app"
        iconName: "package-supported"
        onTriggered: {
          Qt.openUrlExternally(cloud.supportUrl(platform.name()));
        }
      }
    }

    Component {
      id: providerAction
      Kirigami.Action {
        property variant provider
        text: "<b>" + cloud.pretty(provider.type) + "</b>" + "<br/><div>" +
              elided(provider.label, 30) + "</div>"
        onTriggered: {
          pageStack.clear();
          cloud.currently_moved = null;
          cloud.list(cloud.pretty(provider.type), cloud.pretty(provider.type),
                     cloud.root(provider));
        }
      }
    }

    title: "Cloud Browser"
    titleIcon: "qrc:/resources/cloud.png"
    drawerOpen: true
    actions: root.actions(cloud.userProviders.variant())
    height: parent.height - footer_height
    handle.anchors.bottomMargin: footer_height
  }
  contextDrawer: Kirigami.ContextDrawer {
    id: contextDrawer
    height: parent.height - footer_height
    handle.anchors.bottomMargin: footer_height
    title: selected_item ?
             elided(selected_item.filename, 20) :
             "Actions"
  }
  pageStack.initialPage: mainPageComponent
  pageStack.interactive: !fullscreen_player
  pageStack.defaultColumnWidth: 10000
  pageStack.anchors.bottomMargin: footer_height

  Component {
    id: addProviderPage
    RegisterPage {
    }
  }

  Component {
    id: removeProviderPage
    RemoveProvider {
    }
  }

  Component {
    id: listDirectoryPage
    ItemPage {
    }
  }

  Component {
    id: aboutView
    AboutView {
    }
  }

  Component {
    id: mainPageComponent
    Kirigami.ScrollablePage {
      anchors.fill: parent
      title: "Select Cloud Provider"
      Item {}
    }
  }

  Component {
    id: upload_component
    UploadItemRequest {
      property string filename
      property bool upload: true
      property var list

      id: upload_request
      onUploadComplete: {
        var lst = [], i;
        for (i = 0; i < cloud.request.length; i++)
          if (cloud.request[i] !== upload_request)
            lst.push(cloud.request[i]);
          else
            upload_request.destroy();
        cloud.request = lst;
        if (list) list.update(cloud, list.item);
      }
    }
  }

  Component {
    id: download_component
    DownloadItemRequest {
      property string filename
      property bool upload: false

      id: download_request
      onDownloadComplete: {
        var lst = [], i;
        for (i = 0; i < cloud.request.length; i++)
          if (cloud.request[i] !== download_request)
            lst.push(cloud.request[i]);
          else
            download_request.destroy();
        cloud.request = lst;
      }
    }
  }

  Component.onCompleted: {
    platform.initialize(root);
    pageStack.separatorVisible = false;
    if (include_ads)
      platform.showAd();
    if (!cloud.httpServerAvailable) {
      root.showPassiveNotification("Couldn't initialize HTTP server. " +
        "Make sure only one instance of Cloud Browser is running and " +
        "then restart the application.", Infinity);
    }
  }
}
