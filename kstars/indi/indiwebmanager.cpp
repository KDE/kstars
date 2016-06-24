/*  INDI WebManager
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>

#include "indiwebmanager.h"
#include "profileinfo.h"
#include "Options.h"
#include "drivermanager.h"
#include "driverinfo.h"

namespace INDI
{

namespace WebManager
{
   bool getWebManagerResponse(QNetworkAccessManager::Operation operation, const QUrl &url, QJsonDocument *reply, QByteArray *data)
   {
       QNetworkAccessManager manager;
       QNetworkReply *response = NULL;
       QNetworkRequest request;

       request.setUrl(url);

       if (data)
       {
           request.setRawHeader("Content-Type", "application/json");
           request.setRawHeader("Content-Length", QByteArray::number(data->size()));
       }

       switch (operation)
       {
       case QNetworkAccessManager::GetOperation:
           response = manager.get(request);
           break;

       case QNetworkAccessManager::PostOperation:
           if (data)
               response = manager.post(request, *data);
           else
               response = manager.post(request, QByteArray());
           break;

       case QNetworkAccessManager::DeleteOperation:
           response = manager.deleteResource(request);
           break;

       case QNetworkAccessManager::PutOperation:
           response = manager.put(request, *data);
           break;

       default:
           return false;
       }

       // Wait synchronously
       QEventLoop event;
       QObject::connect(response,SIGNAL(finished()),&event,SLOT(quit()));
       event.exec();

       if (response->error() == QNetworkReply::NoError)
       {
           if (reply)
           {
               QJsonParseError parseError;
               *reply = QJsonDocument::fromJson(response->readAll(), &parseError);

                if (parseError.error != QJsonParseError::NoError)
                {
                    qDebug() << "INDI: JSon error during parsing " << parseError.errorString();
                    return false;
                }
           }

            return true;
       }
       else
       {
           qDebug() << "INDI: Error communicating with INDI Web Manager: " << response->errorString();
           return false;
       }
   }

   bool isOnline(ProfileInfo *pi)
   {
       QNetworkAccessManager manager;
       QUrl url(QString("http://%1:%2/api/server/status").arg(pi->host).arg(pi->INDIWebManagerPort));

       QNetworkReply *response = manager.get(QNetworkRequest(url));

       // Wait synchronously
       QEventLoop event;
       QObject::connect(response,SIGNAL(finished()),&event,SLOT(quit()));
       event.exec();

       if (response->error() == QNetworkReply::NoError)
            return true;
       else
           return false;
   }

   bool areDriversRunning(ProfileInfo *pi)
   {
       QUrl url(QString("http://%1:%2/api/server/drivers").arg(pi->host).arg(pi->INDIWebManagerPort));
       QJsonDocument json;

       if (getWebManagerResponse(QNetworkAccessManager::GetOperation, url, &json))
       {
           QJsonArray array = json.array();

           QStringList piExecDrivers;
           QMapIterator<QString, QString> i(pi->drivers);
           while (i.hasNext())
           {
               QString name = i.next().value();
               piExecDrivers << DriverManager::Instance()->findDriverByName(name)->getDriver();
           }

           if (array.count() < piExecDrivers.count())
               return false;

           foreach (QJsonValue value, array)
           {
               QJsonObject obj = value.toObject();

               if (piExecDrivers.contains(obj.value("driver").toString()) == false)
               {
                   qDebug() << "Driver " << obj.value("driver").toString() << " is not running on the remote INDI server.";
                   return false;
               }
           }

           return true;

       }

       return false;
   }


   bool syncProfile(ProfileInfo *pi)
   {
       QUrl url;
       QJsonDocument jsonDoc;
       QByteArray data;

       //Add Profile
       url = QUrl(QString("http://%1:%2/api/profiles/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
       getWebManagerResponse(QNetworkAccessManager::PostOperation, url, NULL);

       // Update profile info
       url = QUrl(QString("http://%1:%2/api/profiles/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
       QJsonObject profileObject { {"port", pi->port } };
       jsonDoc = QJsonDocument(profileObject);
       data = jsonDoc.toJson();
       getWebManagerResponse(QNetworkAccessManager::PutOperation, url, NULL, &data);

       // Add drivers
       url = QUrl(QString("http://%1:%2/api/profiles/%3/drivers").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
       QJsonArray driverArray;
       QMapIterator<QString, QString> i(pi->drivers);
       while (i.hasNext())
       {
           QString name = i.next().value();
           QJsonObject driver;
           driver.insert("label", name);
           driverArray.append(driver);
       }
       jsonDoc = QJsonDocument(driverArray);
       data = jsonDoc.toJson();
       getWebManagerResponse(QNetworkAccessManager::PostOperation, url, NULL, &data);

       return true;
   }

   bool startProfile(ProfileInfo *pi)
   {
       // First make sure profile is created and synced on web manager
       syncProfile(pi);

       // Start profile
       QUrl url(QString("http://%1:%2/api/server/start/%3").arg(pi->host).arg(pi->INDIWebManagerPort).arg(pi->name));
       getWebManagerResponse(QNetworkAccessManager::PostOperation, url, NULL);

       // Make sure drivers are running
       return areDriversRunning(pi);
   }

   bool stopProfile(ProfileInfo *pi)
   {
       // Stop profile
       QUrl url(QString("http://%1:%2/api/server/stop").arg(pi->host).arg(pi->INDIWebManagerPort));
       return getWebManagerResponse(QNetworkAccessManager::PostOperation, url, NULL);

   }
}

}

