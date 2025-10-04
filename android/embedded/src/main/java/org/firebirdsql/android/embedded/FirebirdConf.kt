package org.firebirdsql.android.embedded

import android.content.Context
import android.system.ErrnoException
import android.system.Os
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException


public object FirebirdConf {
    private const val TAG = "FirebirdAndroid"

    @JvmStatic
    @Throws(IOException::class)
    public fun extractAssets(context: Context, force: Boolean = true) {
        val firebirdRootPath = File(context.filesDir, "firebird")

        if (!force && firebirdRootPath.exists())
            return;

        val firebirdTempRootPath = File(context.filesDir, "firebird.tmp")

        if (firebirdTempRootPath.exists())
            deleteDirectory(firebirdTempRootPath)

        firebirdTempRootPath.mkdir()

        val firebirdTmpPath = File(firebirdTempRootPath, "tmp")
        firebirdTmpPath.mkdirs()

        val firebirdLockPath = File(firebirdTempRootPath, "lock")
        firebirdLockPath.mkdirs()

        val assetManager = context.assets
        val buffer = ByteArray(1024)

        for (asset in assetManager.list("firebird")!!) {
            Log.d(TAG, "Extracting Firebird asset: $asset")

            assetManager.open("firebird/$asset").use { input ->
                FileOutputStream(File(firebirdTempRootPath, asset)).use { output ->
                    var len: Int
                    while (input.read(buffer).also { len = it } > 0)
                        output.write(buffer, 0, len)
                    output.flush()
                }
            }
        }

        if (firebirdRootPath.exists())
            deleteDirectory(firebirdRootPath)

        firebirdTempRootPath.renameTo(firebirdRootPath);
    }

    @JvmStatic
    @Throws(ErrnoException::class)
    public fun setEnv(context: Context) {
        val firebirdRootPath = File(context.filesDir, "firebird")
        val firebirdTmpPath = File(firebirdRootPath, "tmp")
        val firebirdLockPath = File(firebirdRootPath, "lock")

        Os.setenv("FIREBIRD", firebirdRootPath.absolutePath, true)
        Os.setenv("FIREBIRD_TMP", firebirdTmpPath.absolutePath, true)
        Os.setenv("FIREBIRD_LOCK", firebirdLockPath.absolutePath, true)
    }

    private fun deleteDirectory(directory: File) {
        for (file in directory.listFiles()) {
            if (file.isDirectory)
                deleteDirectory(file)
            else
                file.delete()
        }
    }
}
