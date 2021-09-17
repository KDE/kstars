/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QFile>
#include <QObject>
#include <QTextStream>

class QString;

/**
 * @class KSFileReader
 * I totally rewrote this because the earlier scheme of reading all the lines of
 * a file into a buffer before processing is actually extremely inefficient
 * because it makes it impossible to interleave file reading and data processing
 * which all modern computers can do.  It also force large data files to be
 * split up into many smaller files which made dealing with the data much more
 * difficult and made the code to read in the data needlessly complicated.  A
 * simple subclassing of QTextStream fixed all of the above problems (IMO).
 *
 * I had added periodic progress reports based on line number to several parts
 * of the code that read in large files.  So I combined that duplicated code
 * into one place which was naturally here.  The progress code causes almost
 * nothing extra to take place when reading a file that does not use it.  The
 * only thing extra is incrementing the line number.  For files that you want to
 * emit periodic progress reports, you call setProgress() once setting up the
 * message to display and the intervals the message will be emitted.  Then
 * inside the loop you call showProgress().  This is an extra call in the read
 * loop but it just does an integer compare and almost always returns.  We could
 * inline it to reduce the call overhead but I don't think it makes a bit of
 * difference considering all of the processing that takes place when we read in
 * a line from a data file.
 *
 * NOTE: We no longer close the file like the previous version did.  I've
 * changed the code where this was assumed.
 *
 * There are two ways to use this class.  One is pass in a QFile& in the
 * constructor which is included only for backward compatibility.  The preferred
 * way is to just instantiate KSFileReader with no parameters and then use the
 * open( QString fname ) method to let this class handle the file opening which
 * helps take unneeded complexity out of the calling classes.  I didn't make a
 * constructor with the filename in it because we need to be able to inform the
 * caller of an error opening the file, hence the bool open(filename) method.
 *
 *
 * -- James B. Bowlin
 */

class KSFileReader : public QObject, public QTextStream
{
    Q_OBJECT

  public:
    /**
     * @short this is the preferred constructor.  You can then use
     * the open() method to let this class open the file for you.
     */
    explicit KSFileReader(qint64 maxLen = 1024);

    /**
     * Constructor
     * @param file is a previously opened (for reading) file.
     * @param maxLen sets the maximum line length before wrapping.  Setting
     * this parameter should help efficiency.  The little max-length.pl
     * script will tell you the maximum line length of files.
     */
    explicit KSFileReader(QFile &file, qint64 maxLen = 1024);

    /**
     * @short opens the file fname from the QStandardPaths::AppDataLocation directory and uses that
     * file for the QTextStream.
     *
     * @param fname the name of the file to open
     * @return returns true on success.  Prints an error message and returns
     * false on failure.
     */
    bool open(const QString &fname);

    /**
     * @short opens the file with full path fname and uses that
     * file for the QTextStream. open() locates QStandardPaths::AppDataLocation behind the scenes,
     * so passing fname such that
     * QString fname = KSPaths::locate(QStandardPaths::AppDataLocation, "file_name" );
     * is equivalent
     *
     * @param fname full path to directory + name of the file to open
     * @return returns true on success.  Prints an error message and returns
     * false on failure.
     */
    bool openFullPath(const QString &fname);

    /**
     * @return true if we are not yet at the end of the file.
     * (I added this to be compatible with existing code.)
     */
    bool hasMoreLines() const { return !QTextStream::atEnd(); }

    /**
     * @short increments the line number and returns the next line from the file as a QString.
     */
    inline QString readLine()
    {
        m_curLine++;
        return QTextStream::readLine(m_maxLen);
    }

    /** @short returns the current line number */
    int lineNumber() const { return m_curLine; }

    /**
     * @short Prepares this instance to emit progress reports on how much
     * of the file has been read (in percent).
     * @param label the label
     * @param lastLine the number of lines to be read
     * @param numUpdates the number of progress reports to send
     */
    void setProgress(QString label, unsigned int lastLine, unsigned int numUpdates = 10);

    /**
     * @short emits progress reports when required and updates bookkeeping
     * for when to send the next report.  This might seem slow at first
     * glance but almost all the time we are just doing an integer compare
     * and returning.  If you are worried about speed we can inline it.
     * It could also safely be included in the readLine() method since
     * m_targetLine is set to MAXUINT in the constructor.
     */
    void showProgress();

  signals:
    void progressText(const QString &message);

  private:
    QFile m_file;
    qint64 m_maxLen;
    unsigned int m_curLine { 0 };

    unsigned int m_totalLines { 0 };
    unsigned int m_targetLine { UINT_MAX };
    unsigned int m_targetIncrement { 0 };
    QString m_label;
};
