package org.videolan.cloudbrowser;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.Context;
import android.graphics.BitmapFactory;
import android.os.Build;

public class NotificationHelper {
    private static NotificationManager m_notification_manager;
    private static final String ActionPlay = "PLAY";
    private static final String ActionPause = "PAUSE";
    private static final String ActionNext = "NEXT";
    private static final int PlayerNotification = 1;

    private static native void callback(String action);

    private static class Receiver extends BroadcastReceiver {
        public void onReceive(Context ctx, Intent intent) {
            callback(intent.getAction());
        }
    }

    public static void initialize() {
        m_notification_manager = (NotificationManager)CloudBrowser.instance().
                getSystemService(Context.NOTIFICATION_SERVICE);
        final IntentFilter filter = new IntentFilter();
        filter.addAction(ActionPlay);
        filter.addAction(ActionPause);
        filter.addAction(ActionNext);
        CloudBrowser.instance().registerReceiver(new Receiver(), filter);
        hidePlayerNotification();
    }

    public static void release() {
        hidePlayerNotification();
    }

    public static void showPlayerNotification(boolean playing, String icon,
        String content_text, String title) {
        final PendingIntent play_intent = PendingIntent.getBroadcast(CloudBrowser.instance(),
                0, new Intent(ActionPlay), PendingIntent.FLAG_UPDATE_CURRENT);
        final PendingIntent pause_intent = PendingIntent.getBroadcast(CloudBrowser.instance(),
                0, new Intent(ActionPause), PendingIntent.FLAG_UPDATE_CURRENT);
        final PendingIntent next_intent = PendingIntent.getBroadcast(CloudBrowser.instance(),
                0, new Intent(ActionNext), PendingIntent.FLAG_UPDATE_CURRENT);
        final Intent intent = new Intent(CloudBrowser.instance(), CloudBrowser.class);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        final PendingIntent content_intent = PendingIntent.getActivity(CloudBrowser.instance(),
                0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder builder = new Notification.Builder(CloudBrowser.instance());
        builder.setSmallIcon(R.drawable.icon).
            setOngoing(playing).
            setContentText(content_text).
            setContentTitle(title).
            setLargeIcon(BitmapFactory.decodeFile(icon)).
            setContentIntent(content_intent);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
            builder.setStyle(new Notification.MediaStyle().setShowActionsInCompactView(0, 1));
        if (!playing)
            builder.addAction(R.drawable.play, ActionPlay, play_intent);
        else
            builder.addAction(R.drawable.pause, ActionPause, pause_intent);
        builder.addAction(R.drawable.next, ActionNext, next_intent);
        m_notification_manager.notify(PlayerNotification, builder.build());
    }

    public static void hidePlayerNotification() {
        m_notification_manager.cancel(PlayerNotification);
    }
}
