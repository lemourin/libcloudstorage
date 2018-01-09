package org.videolan.cloudbrowser;

import android.os.Bundle;
import android.content.Intent;
import android.view.WindowManager;
import android.database.Cursor;
import android.provider.OpenableColumns;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.content.pm.ActivityInfo;
import android.util.Log;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import org.qtproject.qt5.android.bindings.QtActivity;

public class CloudBrowser extends QtActivity {
    private static CloudBrowser m_instance;
    public static AuthView m_auth_view;

    public static class FileOutput {
        public ParcelFileDescriptor m_descriptor;
        public FileOutputStream m_stream;
    }

    public static class FileInput {
        public ParcelFileDescriptor m_descriptor;
        public FileInputStream m_stream;
    }

    public CloudBrowser() {
        m_instance = this;
    }

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        NotificationHelper.initialize();
    }

    @Override
    public void onDestroy() {
        NotificationHelper.release();
        super.onDestroy();
    }

    public static CloudBrowser instance() {
        return m_instance;
    }

    public static void setDefaultOrientation() {
        m_instance.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    public static void setLandScapeOrientation() {
        m_instance.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    public static void enableKeepScreenOn() {
        m_instance.runOnUiThread(new Runnable() {
            public void run() {
                m_instance.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public static void disableKeepScreenOn() {
        m_instance.runOnUiThread(new Runnable() {
            public void run() {
                m_instance.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); 
            }
        });
    }

    public static Intent openFileDialog() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        return intent;
    }

    public static Intent createFileDialog(String filename) {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.putExtra(Intent.EXTRA_TITLE, filename);
        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        return intent;
    }

    public static Intent openWebPage(String url) {
        Intent intent = new Intent(m_instance, AuthView.class);
        intent.putExtra("address", url);
        return intent;
    }

    public static void closeWebPage(Intent intent) {
        if (m_auth_view != null) {
            m_auth_view.setResult(1, intent);
            m_auth_view.finish();
            m_auth_view = null;
        }
    }

    public static Uri stringToUri(String uri) {
        String basename = uri.substring(0, uri.lastIndexOf('/') + 1);
        String filename = uri.substring(uri.lastIndexOf('/') + 1);
        return Uri.parse(basename + Uri.encode(Uri.decode(filename)));
    }

    public static FileOutput openWrite(String uri) throws IOException {
        FileOutput file = new FileOutput();
        file.m_descriptor = m_instance.getContentResolver().
            openFileDescriptor(stringToUri(uri), "w");
        file.m_stream = new FileOutputStream(file.m_descriptor.getFileDescriptor());
        return file;
    }

    public static FileInput openRead(String uri) throws IOException {
        FileInput file = new FileInput();
        file.m_descriptor = m_instance.getContentResolver().
            openFileDescriptor(stringToUri(uri), "r");
        file.m_stream = new FileInputStream(file.m_descriptor.getFileDescriptor());
        return file;
    }

    public static void write(FileOutput file, byte[] data) throws IOException {
        file.m_stream.write(data);
    }

    public static byte[] read(FileInput file, int size) throws IOException {
        byte[] buffer = new byte[size];
        int nsize = file.m_stream.read(buffer);
        if (nsize == -1)
            return new byte[0];
        byte[] result = new byte[nsize];
        for (int i = 0; i < nsize; i++)
            result[i] = buffer[i];
        return result;
    }

    public static String filename(Uri uri) {
        Cursor cursor = m_instance.getContentResolver().
            query(uri, null, null, null, null, null);
        String result = "";
        try {
            if (cursor != null && cursor.moveToFirst())
                result = cursor.getString(
                    cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
        } finally {
            cursor.close();
        }
        return result;
    }

    public static long size(FileInput file) throws IOException {
        return file.m_descriptor.getStatSize();
    }

    public static boolean reset(FileInput file) throws IOException {
        file.m_stream.close();
        file.m_stream = new FileInputStream(file.m_descriptor.getFileDescriptor());
        return true;
    }

    public static void closeWrite(FileOutput file) throws IOException {
        file.m_stream.close();
        file.m_descriptor.close();
    }

    public static void closeRead(FileInput file) throws IOException {
        file.m_stream.close();
        file.m_descriptor.close();
    }
}
