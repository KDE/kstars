#ifndef DOWNLOADHELPER_H
#define DOWNLOADHELPER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QMessageBox>

#include <kauth.h>

using namespace KAuth;

class DownloadHelper : public QObject
{
    Q_OBJECT

    public Q_SLOTS:
        ActionReply saveindexfile(const QVariantMap &args);
        ActionReply removeindexfileset(const QVariantMap &args);

};

#endif // DOWNLOADHELPER_H
