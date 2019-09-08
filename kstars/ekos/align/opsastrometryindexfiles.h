
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

  protected:
    void showEvent(QShowEvent *) override;

  public slots:
    void slotUpdate();
    void slotOpenIndexFileDirectory();
    void downloadOrDeleteIndexFiles(bool checked);

  private:
    void downloadIndexFile(const QString &URL, const QString &fileN, QCheckBox *checkBox, int currentIndex,
                           int maxIndex, double fileSize);
    bool astrometryIndicesAreAvailable();
    void setDownloadInfoVisible(QString indexSeriesName,QCheckBox *checkBox, bool set);
    bool fileCountMatches(QDir directory, QString indexName);
    void disconnectDownload(QMetaObject::Connection *cancelConnection, QMetaObject::Connection *replyConnection, QMetaObject::Connection *percentConnection);

    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };
    QNetworkAccessManager *manager { nullptr };
    QMap<float, QString> astrometryIndex;
    QTimer timeoutTimer;
    int downloadSpeed { 0 }; //bytes per millisecond
    int actualdownloadSpeed { 0 }; //bytes per millisecond
};
}
