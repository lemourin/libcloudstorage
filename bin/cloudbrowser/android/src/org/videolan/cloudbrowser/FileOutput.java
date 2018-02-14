package org.videolan.cloudbrowser;

import android.os.ParcelFileDescriptor;
import android.content.Context;

import java.io.FileOutputStream;
import java.io.IOException;

public class FileOutput {
    private ParcelFileDescriptor m_descriptor;
    private FileOutputStream m_stream;

    public FileOutput(Context context, String uri) throws IOException {
        m_descriptor = context.getContentResolver().
            openFileDescriptor(Utility.stringToUri(uri), "w");
        m_stream = new FileOutputStream(m_descriptor.getFileDescriptor());
    }

    public void write(byte[] data) throws IOException {
        m_stream.write(data);
    }

    public void close() throws IOException {
        m_stream.close();
        m_descriptor.close();
    }
}
