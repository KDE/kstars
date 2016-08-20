#include "auxiliary/kspaths.h"
#include <QFileInfo>
#include <QDebug>

QString KSPaths::locate(QStandardPaths::StandardLocation location, const QString &fileName,
                        QStandardPaths::LocateOptions options)
{
    QString dir;
    if(location == QStandardPaths::GenericDataLocation) dir = "kstars" + QDir::separator();
#ifdef ANDROID
    QString file = QStandardPaths::locate(location, dir + fileName ,options);
    if(file.isEmpty()) {
        file = "/data/data/org.kde.kstars/qt-reserved-files/share/kstars/" + fileName;
        if (!QFileInfo(file).exists()) {
            return QString();
        }
    }
    return file;
#else
    return QStandardPaths::locate(location, dir + fileName,options);
#endif
}

QStringList KSPaths::locateAll(QStandardPaths::StandardLocation location, const QString &fileName,
                        QStandardPaths::LocateOptions options) {
    QString dir;
    if(location == QStandardPaths::GenericDataLocation) dir = "kstars" + QDir::separator();
#ifdef ANDROID
    QStringList file = QStandardPaths::locateAll(location, dir + fileName,options);
    if(file.isEmpty()) {
        QString f = "/data/data/org.kde.kstars/qt-reserved-files/share/kstars/" + fileName;
        if (QFileInfo(f).exists()) {
            file.append(f);
        }
    }
    return file;
#else
    return QStandardPaths::locateAll(location, dir + fileName,options);
#endif
}

QString KSPaths::writableLocation(QStandardPaths::StandardLocation type) {
    QString dir;
    if(type == QStandardPaths::GenericDataLocation) dir = "kstars" + QDir::separator();
    return QStandardPaths::writableLocation(type) + QDir::separator() + dir;
}

