#include "auxiliary/kspaths.h"

QString KSPaths::locate(QStandardPaths::StandardLocation location, const QString &fileName,
                        QStandardPaths::LocateOptions options) {
#ifdef ANDROID
    QString file = QStandardPaths::locate(location,fileName,options);
    if(file.isEmpty()) {
        file = "/data/data/org.kde.kstars/qt-reserved-files/share/kstars/" + fileName;
    }
    return file;
#else
    return QStandardPaths::locate(location,fileName,options);
#endif
}

QStringList KSPaths::locateAll(QStandardPaths::StandardLocation location, const QString &fileName,
                        QStandardPaths::LocateOptions options) {
#ifdef ANDROID
    QStringList file = QStandardPaths::locateAll(location,fileName,options);
    if(file.isEmpty()) {
        file[0] = "/data/data/org.kde.kstars/qt-reserved-files/share/kstars/" + fileName;
    }
    return file;
#else
    return QStandardPaths::locateAll(location,fileName,options);
#endif
}
