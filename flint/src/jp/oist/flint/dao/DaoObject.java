/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */
package jp.oist.flint.dao;

import org.sqlite.SQLiteConfig;
import java.io.File;
import java.io.IOException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;

public abstract class DaoObject implements AutoCloseable {

    static {
        try {
            Class.forName("org.sqlite.JDBC");
        } catch (ClassNotFoundException ex) {
            // ignored.
        }
    }

    /**
     * The working directory containing the database file.
     */
    protected final File mWorkingDir;

    /**
     * The database file.
     */
    private final File mDatabaseFile;

    /**
     * The cached connection.
     */
    private Connection mConnection;

    protected DaoObject(String dbName, File workingDir) {
        assert dbName != null;
        assert !dbName.isEmpty();

        mWorkingDir = workingDir;
        mDatabaseFile = new File(workingDir, dbName);
        mConnection = null;
    }

    private void connect() throws SQLException, IOException {
        assert mConnection == null;
        String dsn = String.format("jdbc:sqlite:%s", mDatabaseFile.getCanonicalPath());
        SQLiteConfig config = new SQLiteConfig();
        config.setReadOnly(true);
        mConnection = DriverManager.getConnection(dsn, config.toProperties());
    }

    public synchronized Connection getConnection()
            throws SQLException, IOException {
        if (mConnection == null)
            connect();
        return mConnection;
    }

    @Override
    public void close () throws SQLException {
        if (mConnection != null)
            mConnection.close();

    }
}
