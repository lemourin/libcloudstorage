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
    private Context m_context;
    private NotificationManager m_notification_manager;
    private Receiver m_receiver;

    private static final String ActionPlay = "PLAY";
    private static final String ActionPause = "PAUSE";
    private static final String ActionNext = "NEXT";
    public static final int PlayerNotification = 1;

    private static class Receiver extends BroadcastReceiver {
        public void onReceive(Context ctx, Intent intent) {
            Utility.onActionRequested(intent.getAction());
        }
    }

    NotificationHelper(Context context) {
        m_context = context;
        m_notification_manager =
            (NotificationManager)context.getSystemService(Context.NOTIFICATION_SERVICE);
        m_receiver = new Receiver();
        final IntentFilter filter = new IntentFilter();
        filter.addAction(ActionPlay);
        filter.addAction(ActionPause);
        filter.addAction(ActionNext);
        context.registerReceiver(m_receiver, filter);
        hidePlayerNotification();

        m_context.startService(
            new Intent(context, NotificationService.class)
        );
    }

    public void release() {
        m_context.unregisterReceiver(m_receiver);
        hidePlayerNotification();
    }

    public void showPlayerNotification(
        boolean playing,
        String icon,
        String content_text,
        String title
    ) {
        final PendingIntent play_intent = PendingIntent.getBroadcast(m_context,
                0, new Intent(ActionPlay), PendingIntent.FLAG_UPDATE_CURRENT);
        final PendingIntent pause_intent = PendingIntent.getBroadcast(m_context,
                0, new Intent(ActionPause), PendingIntent.FLAG_UPDATE_CURRENT);
        final PendingIntent next_intent = PendingIntent.getBroadcast(m_context,
                0, new Intent(ActionNext), PendingIntent.FLAG_UPDATE_CURRENT);
        final Intent intent = new Intent(m_context, CloudBrowser.class);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setAction(Intent.ACTION_MAIN);
        final PendingIntent content_intent = PendingIntent.getActivity(m_context,
                0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder builder = new Notification.Builder(m_context);
        builder.setSmallIcon(R.drawable.icon).
            setOngoing(playing).
            setContentText(content_text).
            setContentTitle(title).
            setLargeIcon(BitmapFactory.decodeFile(icon)).
            setContentIntent(content_intent);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
            builder.setStyle(new Notification.MediaStyle().setShowActionsInCompactView(0, 1));
        if (!playing)
            builder.addAction(android.R.drawable.ic_media_play, ActionPlay, play_intent);
        else
            builder.addAction(android.R.drawable.ic_media_pause, ActionPause, pause_intent);
        builder.addAction(android.R.drawable.ic_media_next, ActionNext, next_intent);

        m_notification_manager.notify(PlayerNotification, builder.build());
    }

    public void hidePlayerNotification() {
        m_notification_manager.cancel(PlayerNotification);
    }
}
