/***************************************************************************
                          astrobinapi.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 8 2012
    copyright            : (C) 2012 by Lukasz Jaskiewicz and Rafal Kulaga
    email                : lucas.jaskiewicz@gmail.com, rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "astrobinapi.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QDebug>

#include "kinputdialog.h"

AstroBinApi::AstroBinApi(QNetworkAccessManager *manager, QObject *parent)
    : QObject(parent), m_NetworkManager(manager), m_ResultsLimit(20), m_Offset(0)
{
    m_UrlBase = "http://www.astrobin.com/api/v1/";

    m_ApiKey = "ad77ef2a21e5b1bca8ff1d513e4be74746e912dd"; //KInputDialog::getText("Astrobin Api Key", "Enter Key");
    m_ApiSecret = "ab40f034595be072eec052687d4656de3c361415"; //KInputDialog::getText("Astrobin Secret", "Enter Secret");

    connect(m_NetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
}

AstroBinApi::~AstroBinApi()
{}

void AstroBinApi::searchImageOfTheDay()
{
    QString requestStub = "imageoftheday/?";
    astroBinRequestFormatAndSend(requestStub);
}

void AstroBinApi::searchObjectImages(const QString &name)
{
    QString requestStub = "image/?subject=" + name;
    astroBinRequestFormatAndSend(requestStub);
}

void AstroBinApi::searchTitleContaining(const QString &string)
{
    QString requestStub = "image/?title__icontains=" + string;
    astroBinRequestFormatAndSend(requestStub);
}

void AstroBinApi::searchDescriptionContaining(const QString &string)
{
    QString requestStub = "image/?description__icontains=" + string;
    astroBinRequestFormatAndSend(requestStub);
}

void AstroBinApi::searchUserImages(const QString &user)
{
    QString requestStub = "image/?user=" + user;
    astroBinRequestFormatAndSend(requestStub);
}

void AstroBinApi::astroBinRequestFormatAndSend(const QString &requestStub)
{
    QUrl requestUrl(m_UrlBase + requestStub +
                    + "&limit=" + QString::number(m_ResultsLimit)
                    + "&offset=" + QString::number(m_Offset)
                    + "&api_key=" + m_ApiKey
                    + "&api_secret=" + m_ApiSecret
                    + m_UrlApiTypeEnding);

    QNetworkRequest request(requestUrl);
    request.setOriginatingObject(this);
    m_NetworkManager->get(request);
}


