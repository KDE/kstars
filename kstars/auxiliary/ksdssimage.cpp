/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksdssimage.h"

#include <QImageReader>

KSDssImage::KSDssImage(const QString &fileName)
{
    m_FileName = fileName;
    QImageReader reader(
        m_FileName); // FIXME: Need a good way to tell whether we are dealing with a metadata-ful image or not
    m_Metadata.format =
        (reader.format().toLower().contains("png") ? KSDssImage::Metadata::PNG : KSDssImage::Metadata::GIF);
    if (reader.text("Author").contains("KStars")) // Must have metadata
    {
        m_Metadata.valid   = true;
        m_Metadata.src     = (KSDssImage::Metadata::Source)reader.text("Source").toInt();
        m_Metadata.version = reader.text("Version");
        m_Metadata.object  = reader.text("Object");
        m_Metadata.ra0.setFromString(reader.text("RA"), false);
        m_Metadata.dec0.setFromString(reader.text("Dec"), true);
        m_Metadata.width  = reader.text("Width").toFloat();
        m_Metadata.height = reader.text("Height").toFloat();
        QString band      = reader.text("Band");
        if (!band.isEmpty())
            m_Metadata.band = band.at(0).toLatin1();
        m_Metadata.gen = reader.text("Generation").toInt();
    }
    m_Image = reader.read();
}
