#ifndef OPSASTROMETRYINDEXFILES_H
#define OPSASTROMETRYINDEXFILES_H

#include <QDialog>
#include "ui_opsastrometryindexfiles.h"
#include "QNetworkAccessManager"


class KConfigDialog;

namespace Ekos
{

class Align;

class OpsAstrometryIndexFiles : public QDialog, public Ui::OpsAstrometryIndexFiles
{
        Q_OBJECT

    public:
        explicit OpsAstrometryIndexFiles(Align * parent);
        ~OpsAstrometryIndexFiles();

    protected:
        void showEvent(QShowEvent *);

    public slots:
        void slotUpdate();
        void slotOpenIndexFileDirectory();
        void downloadOrDeleteIndexFiles(bool checked);

    private:
        KConfigDialog * m_ConfigDialog;
        Align * alignModule;
        QNetworkAccessManager * manager;
        bool getAstrometryDataDir(QString &dataDir);
        void downloadIndexFile(QString URL, QString fileN, QCheckBox *checkBox, int currentIndex, int maxIndex);
        QMap<float, QString> astrometryIndex;
        bool astrometryIndicesAreAvailable();

};

}

#endif // OPSASTROMETRYINDEXFILES_H
