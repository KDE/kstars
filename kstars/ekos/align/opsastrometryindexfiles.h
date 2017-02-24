#ifndef OPSASTROMETRYINDEXFILES_H
#define OPSASTROMETRYINDEXFILES_H

#include <QDialog>
#include "ui_opsastrometryindexfiles.h"


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

private:
    KConfigDialog *m_ConfigDialog;
    Align *alignModule;
    bool getAstrometryDataDir(QString &dataDir);
    QMap<float, QString> astrometryIndex;
};

}

#endif // OPSASTROMETRYINDEXFILES_H
