/***************************************************************************
                          finddialoglite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 29 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "detaildialoglite.h"
#include "skymaplite.h"
#include "kstarslite/skyobjectlite.h"
#include "ksutils.h"
#include "ksplanetbase.h"
#include "ksmoon.h"
#include "supernova.h"

#include "skymapcomposite.h"
#include "constellationboundarylines.h"

DetailDialogLite::DetailDialogLite( )
{

}

void DetailDialogLite::initialize() {
    connect(SkyMapLite::Instance(), SIGNAL(objectLiteChanged()), this, SLOT(createGeneralTab()));
}

void DetailDialogLite::createGeneralTab() {
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();

    // Stuff that should be visible only for specific types of objects
    setProperty("illumination", "");// Only shown for the moon
    setProperty("BVindex",""); // Only shown for stars
    setupThumbnail();

    //Fill in the data fields
    //Contents depend on type of object
    QString objecttyp, str;

    switch (selectedObject->type()) {
        case SkyObject::STAR: {
            StarObject* s = (StarObject*) selectedObject;

            if (s->getHDIndex()) {
                setProperty("name",(QString("%1, HD %2").arg(s->longname()).arg(s->getHDIndex())));
            } else {
                setProperty("name",s->longname());
            }

            objecttyp = s->sptype() + ' ' + i18n("star");
            setProperty("magnitude",i18nc("number in magnitudes", "%1 mag",
                                          QLocale().toString(s->mag(), 'f', 2)));  //show to hundredth place

            if(s->getBVIndex() < 30.) {
                setProperty("BVindex",QString::number(s->getBVIndex() , 'f', 2));
            }

            //distance
            if (s->distance() > 2000. || s->distance() < 0.) { // parallax < 0.5 mas
                setProperty("distance",(QString(i18nc("larger than 2000 parsecs", "> 2000 pc"))));
            } else if (s->distance() > 50.) { //show to nearest integer
                setProperty("distance",(i18nc("number in parsecs", "%1 pc",
                                              QLocale().toString( s->distance(), 'f', 0))));
            } else if (s->distance() > 10.0) { //show to tenths place
                setProperty("distance",(i18nc("number in parsecs", "%1 pc",
                                              QLocale().toString( s->distance(), 'f', 1))));
            } else { //show to hundredths place
                setProperty("distance",(i18nc("number in parsecs", "%1 pc",
                                              QLocale().toString(s->distance(), 'f', 2 ))));
            }

            //Note multiplicity/variablility in angular size label
            setProperty("angSize",QString());
            if (s->isMultiple() && s->isVariable()) {
                QString multiple = QString(i18nc( "the star is a multiple star", "multiple" ) + ',');
                setProperty("angSize", QString(multiple + "\n" + (i18nc( "the star is a variable star", "variable" ))));
            } else if (s->isMultiple()) {
                setProperty("angSize",i18nc("the star is a multiple star", "multiple"));
            } else if (s->isVariable()) {
                setProperty("angSize",(i18nc("the star is a variable star", "variable")));
            }

            break; //end of stars case
        }
        case SkyObject::ASTEROID:  //[fall through to planets]
        case SkyObject::COMET:     //[fall through to planets]
        case SkyObject::MOON:      //[fall through to planets]
        case SkyObject::PLANET: {
            KSPlanetBase* ps = (KSPlanetBase*) selectedObject;

            setProperty("name",ps->longname());

            //Type is "G5 star" for Sun
            if (ps->name() == "Sun") {
                objecttyp = i18n("G5 star");
            } else if (ps->name() == "Moon") {
                objecttyp = ps->translatedName();
            } else if (ps->name() == i18n("Pluto") || ps->name() == "Ceres" || ps->name() == "Eris") { // TODO: Check if Ceres / Eris have translations and i18n() them
                objecttyp = i18n("Dwarf planet");
            } else {
                objecttyp = ps->typeName();
            }

            //The moon displays illumination fraction and updateMag is called to calculate moon's current magnitude
            if (selectedObject->name() == "Moon") {
                setProperty("illumination",(QString("%1 %").arg(QLocale().toString(((KSMoon*)selectedObject)->illum()*100., 'f', 0))));
                ((KSMoon *)selectedObject)->updateMag();
            }

            // JM: Shouldn't we use the calculated magnitude? Disabling the following
            /*
                if(selectedObject->type() == SkyObject::COMET){
                    Data->Magnitude->setText(i18nc("number in magnitudes", "%1 mag",
                                             QLocale().toString( ((KSComet *)selectedObject)->getTotalMagnitudeParameter(), 'f', 2)));  //show to hundredth place

                }
                else{*/
            setProperty("magnitude",(i18nc("number in magnitudes", "%1 mag",
                                           QLocale().toString(ps->mag(), 'f', 2))));  //show to hundredth place
            //}

            //Distance from Earth.  The moon requires a unit conversion
            if (ps->name() == "Moon") {
                setProperty("distance",(i18nc("distance in kilometers", "%1 km",
                                              QLocale().toString(ps->rearth()*AU_KM, 'f', 2))));
            } else {
                setProperty("distance",(i18nc("distance in Astronomical Units", "%1 AU",
                                              QLocale().toString( ps->rearth(), 'f', 3))));
            }

            //Angular size; moon and sun in arcmin, others in arcsec
            if (ps->angSize()) {
                if (ps->name() == "Sun" || ps->name() == "Moon") {
                    setProperty("angSize",(i18nc("angular size in arcminutes", "%1 arcmin",
                                                 QLocale().toString(ps->angSize(), 'f', 1)))); // Needn't be a plural form because sun / moon will never contract to 1 arcminute
                } else {
                    setProperty("angSize",i18nc("angular size in arcseconds","%1 arcsec",
                                                QLocale().toString( ps->angSize()*60.0 , 'f', 1)));
                }
            } else {
                setProperty("angSize", "--" );
            }

            break; //end of planets/comets/asteroids case
        }
        case SkyObject::SUPERNOVA: {
            Supernova* sup=(Supernova*) selectedObject;

            objecttyp = i18n("Supernova");
            setProperty("name",sup->name());
            setProperty("magnitude",(i18nc("number in magnitudes", "%1 mag",
                                           QLocale().toString(sup->mag(), 'f', 2))));
            setProperty("distance", "---");
            break;
        }
        default: { //deep-sky objects
            DeepSkyObject* dso = (DeepSkyObject*) selectedObject;

            //Show all names recorded for the object
            QStringList nameList;
            if (!dso->longname().isEmpty() && dso->longname() != dso->name()) {
                nameList.append(dso->translatedLongName());
                nameList.append(dso->translatedName());
            } else {
                nameList.append(dso->translatedName());
            }

            if (!dso->translatedName2().isEmpty()) {
                nameList.append(dso->translatedName2());
            }

            if (dso->ugc() != 0) {
                nameList.append(QString("UGC %1").arg(dso->ugc()));
            }

            if (dso->pgc() != 0) {
                nameList.append(QString("PGC %1").arg(dso->pgc()));
            }

            setProperty("name",nameList.join(","));

            objecttyp = dso->typeName();

            if (dso->type() == SkyObject::RADIO_SOURCE)
            {
                //ta->MagLabel->setText(i18nc("integrated flux at a frequency", "Flux(%1):", dso->customCatalog()->fluxFrequency()));
                //Data->Magnitude->setText(i18nc("integrated flux value", "%1 %2",
                  //                             QLocale().toString(dso->flux(), 'f', 1), dso->customCatalog()->fluxUnit()));  //show to tenths place
            } else if (dso->mag() > 90.0) {
                setProperty("magnitude", "--");
            } else {
                setProperty("magnitude",i18nc("number in magnitudes", "%1 mag",
                                               QLocale().toString( dso->mag(), 'f', 1)));  //show to tenths place
            }

            //No distances at this point...
            setProperty("distance", "--");

            //Only show decimal place for small angular sizes
            if (dso->a() > 10.0) {
                setProperty("angSize", i18nc("angular size in arcminutes", "%1 arcmin",
                                             QLocale().toString(dso->a(), 'f', 0)));
            } else if (dso->a()) {
                setProperty("angSize",i18nc("angular size in arcminutes", "%1 arcmin",
                                             QLocale().toString( dso->a(), 'f', 1 )));
            } else {
                setProperty("angSize", "--");
            }

            break;
        }
    }

    //Common to all types:
    QString cname = KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(selectedObject);
    if (selectedObject->type() != SkyObject::CONSTELLATION) {
        cname = i18nc("%1 type of sky object (planet, asteroid etc), %2 name of a constellation", "%1 in %2", objecttyp, cname);
    }
    setProperty("typeInConstellation", cname);
}

void DetailDialogLite::setupThumbnail() {
    SkyObject *selectedObject = SkyMapLite::Instance()->getClickedObjectLite()->getObject();
    //No image if object is a star
    if ( selectedObject->type() == SkyObject::STAR ||
         selectedObject->type() == SkyObject::CATALOG_STAR ) {
        /*Thumbnail->scaled( Data->Image->width(), Data->Image->height() );
        Thumbnail->fill( Data->DataFrame->palette().color( QPalette::Window ) );
        Data->Image->setPixmap( *Thumbnail );*/
        setProperty("thumbnail", "");
        return;
    }

    //Try to load the object's image from disk
    //If no image found, load "no image" image
    QFile file;
    QString fname = "thumb-" + selectedObject->name().toLower().remove( ' ' ) + ".png";
    if ( KSUtils::openDataFile( file, fname ) ) {
        file.close();
        setProperty("thumbnail", file.fileName());
    } else {
        setProperty("thumbnail", "");
    }
}

