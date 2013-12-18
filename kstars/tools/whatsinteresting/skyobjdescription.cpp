
#include <QString>
#include <QUrl>
#include <QDebug>

#include "skyobjdescription.h"


SkyObjDescription::SkyObjDescription(const QString so_Name, const QString so_Type): soName(so_Name), soType(so_Type), m_description(""), m_DownloadedData("")
{
    QUrl wikiUrl("http://en.wikipedia.org/w/api.php?action=opensearch&search="+ soName.replace(" ", "_").toLower() + "_" + soType.toLower() + "&format=xml&limit=1.xml");

    QNetworkRequest request(wikiUrl);

    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)), SLOT(fileDownloaded(QNetworkReply*)));
    manager->get(request);

}

SkyObjDescription::~SkyObjDescription()
{

}

void SkyObjDescription::fileDownloaded(QNetworkReply* reply)
{
    m_DownloadedData = reply->readAll();

    if(m_DownloadedData != ""){
        QString data(m_DownloadedData);
        if(data.contains("description", Qt::CaseInsensitive))
        {
            int startIndex = data.lastIndexOf("<Description xml:space=\"preserve\">") + 34;
            int endIndex = data.lastIndexOf("</Description>");
            m_description = data.mid(startIndex, endIndex-startIndex);
        }
    }
    reply->deleteLater();
    emit downloaded();

}

QString SkyObjDescription::downloadedData() const
{
    return m_description;
}
