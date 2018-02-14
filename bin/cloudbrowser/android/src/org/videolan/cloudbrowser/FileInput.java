package org.videolan.cloudbrowser;

import android.os.ParcelFileDescriptor;
import android.content.Context;

import java.io.FileInputStream;
import java.io.IOException;

public class FileInput {
    private ParcelFileDescriptor m_descriptor;
    private FileInputStream m_stream;

    public FileInput(Context context, String uri) throws IOException {
        m_descriptor = context.getContentResolver().
            openFileDescriptor(Utility.stringToUri(uri), "r");
        m_stream = new FileInputStream(m_descriptor.getFileDescriptor());
    }

    public long size() throws IOException {
        return m_descriptor.getStatSize();
    }

    public boolean reset() throws IOException {
        m_stream.close();
        m_stream = new FileInputStream(m_descriptor.getFileDescriptor());
        return true;
    }

    public void close() throws IOException {
        m_stream.close();
        m_descriptor.close();
    }

    public byte[] read(int size) throws IOException {
        byte[] buffer = new byte[size];
        int nsize = m_stream.read(buffer);
        if (nsize == -1)
            return new byte[0];
        byte[] result = new byte[nsize];
        for (int i = 0; i < nsize; i++)
            result[i] = buffer[i];
        return result;
    }
}
