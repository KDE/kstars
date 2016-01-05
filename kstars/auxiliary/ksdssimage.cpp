/***************************************************************************
                     ksdssimage.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 05 Jan 2016 00:29:22 CST
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/* Project Includes */
#include "ksdssimage.h"

/* KDE Includes */

/* Qt Includes */
#include <QString>
#include <QChar>
#include <QImageReader>
#include <QImage>

KSDssImage::KSDssImage( const QString &fileName ) {
    m_FileName = fileName;
    QImageReader reader( m_FileName );
    m_Metadata.src = (KSDssImage::Metadata::Source) reader.text( "Source" ).toInt();
    m_Metadata.format = ( reader.format().toLower().contains( "png" ) ? KSDssImage::Metadata::PNG : KSDssImage::Metadata::GIF );
    m_Metadata.version = reader.text( "Version" );
    m_Metadata.object = reader.text( "Object" );
    m_Metadata.ra0.setFromString( reader.text( "RA" ), false );
    m_Metadata.dec0.setFromString( reader.text( "Dec" ), true );
    m_Metadata.width = reader.text( "Width" ).toFloat();
    m_Metadata.height = reader.text( "Height" ).toFloat();
    m_Metadata.band = reader.text( "Band" ).at(0).toLatin1();
    m_Metadata.gen = reader.text( "Generation" ).toInt();

    m_Image = reader.read();
}
