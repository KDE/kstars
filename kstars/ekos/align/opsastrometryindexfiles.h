
#pragma once

#include "ui_opsastrometryindexfiles.h"

#include <QDialog>
#include <QMap>
#include <QString>

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
    ~OpsAstrometryIndexFiles();

  protected:
    void showEvent(QShowEvent *);

  public slots:
    void slotUpdate();
    void slotOpenIndexFileDirectory();
    void downloadOrDeleteIndexFiles(bool checked);

  private:
    bool getAstrometryDataDir(QString &dataDir);
    void downloadIndexFile(const QString &URL, const QString &fileN, QCheckBox *checkBox, int currentIndex,
                           int maxIndex, double fileSize);
    bool astrometryIndicesAreAvailable();

    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };
    QNetworkAccessManager *manager { nullptr };
    QMap<float, QString> astrometryIndex;
};
}
