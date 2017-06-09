#include "downloadhelper.h"

ActionReply DownloadHelper::saveindexfile(const QVariantMap &args)
{
    ActionReply reply;
    QString filename = args["filename"].toString();
    QFile file(filename);

    if (file.open(QIODevice::WriteOnly) == false)
    {
        reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(file.errorString());
        return reply;
    }

    QByteArray array = args["contents"].toByteArray();

    file.write(array.data(),array.size());
    file.close();

    return reply;
}

ActionReply DownloadHelper::removeindexfileset(const QVariantMap &args)
{
    ActionReply reply;
    QString indexSetName = args["indexSetName"].toString();
    QString astrometryDataDir = args["astrometryDataDir"].toString();

    QStringList nameFilter("*.fits");
    QDir directory(astrometryDataDir);
    QStringList indexList = directory.entryList(nameFilter);
    foreach(QString fileName, indexList)
    {
        if(fileName.contains(indexSetName.left(10)))
        {
            if(!directory.remove(fileName))
            {
                reply = ActionReply::HelperErrorReply();
                reply.setErrorDescription("File did not delete");
                return reply;
            }
        }
    }

    return reply;
}


KAUTH_HELPER_MAIN("org.kde.kf5auth.kstars" , DownloadHelper);
