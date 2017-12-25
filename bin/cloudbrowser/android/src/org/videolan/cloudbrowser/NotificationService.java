package org.videolan.cloudbrowser;

import android.app.Service;
import android.app.NotificationManager;
import android.os.IBinder;
import android.content.Intent;
import android.content.Context;

public class NotificationService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onTaskRemoved(Intent intent) {
        NotificationManager notification_manager =
            (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        notification_manager.cancel(NotificationHelper.PlayerNotification);
        stopSelf();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_NOT_STICKY;
    }
}
