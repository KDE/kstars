
#pragma once

#include "ui_opsastrometryindexfiles.h"

#include <QDialog>
#include <QMap>
#include <QString>
#include <QDir>
#include <QTimer>

class QNetworkAccessManager;

class Align;
class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAstrometryIndexFiles : public QDialog, public Ui::OpsAstrometryIndexFiles
{
        Q_OBJECT

    public:
        explicit OpsAstrometryIndexFiles(Align *parent);
        virtual ~OpsAstrometryIndexFiles() override = default;

        Q_SCRIPTABLE void downloadSingleIndexFile(const QString &indexFileName);

    protected:
        void showEvent(QShowEvent *) override;

    public slots:
        void slotUpdate();
        void slotOpenIndexFileDirectory();
        void downloadOrDeleteIndexFiles(bool checked);
        void addDirectoryToList(QString directory);
        void removeDirectoryFromList(QString directory);
        void updateIndexDirectoryList();
        void setCheckBoxStateProgrammatically(QCheckBox* checkbox, bool checked);

    signals:
        void newDownloadProgress(QString info);

    private:
        void downloadIndexFile(const QString &URL, const QString &fileN, const QString &indexSeriesName, int currentIndex,
                               int maxIndex, double fileSize);
        bool astrometryIndicesAreAvailable();
        void setDownloadInfoVisible(const QString &indexSeriesName, bool set);
        int indexFileCount(QString indexName);
        bool fileCountMatches(QDir directory, QString indexName);
        void disconnectDownload(QMetaObject::Connection *cancelConnection, QMetaObject::Connection *replyConnection,
                                QMetaObject::Connection *percentConnection);
        QString findFirstWritableDir();

        KConfigDialog *m_ConfigDialog { nullptr };
        Align *alignModule { nullptr };
        QNetworkAccessManager *manager { nullptr };
        QMap<float, QString> astrometryIndex;
        QTimer timeoutTimer;
        int downloadSpeed { 0 }; //bytes per millisecond
        int actualdownloadSpeed { 0 }; //bytes per millisecond
};
}
