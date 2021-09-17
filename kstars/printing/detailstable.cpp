/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "detailstable.h"

#include <QTextDocument>
#include <QTextTable>

#include "kstars.h"
#include "kstarsdata.h"
#include "ksplanetbase.h"
#include "starobject.h"
#include "skymapcomposite.h"
#include "constellationboundarylines.h"
#include "catalogobject.h"
#include "ksmoon.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "Options.h"
#include "catalogsdb.h"

DetailsTable::DetailsTable()
{
    m_Document = new QTextDocument(KStars::Instance());

    setDefaultFormatting();
}

DetailsTable::~DetailsTable()
{
    if (m_Document)
    {
        delete m_Document;
    }
}

void DetailsTable::createGeneralTable(SkyObject *obj)
{
    clearContents();

    QTextCursor cursor = m_Document->rootFrame()->firstCursorPosition();

    //Fill in the data fields
    //Contents depend on type of object
    StarObject *s      = nullptr;
    CatalogObject *dso = nullptr;
    KSPlanetBase *ps   = nullptr;
    QString pname, oname;

    QString objNamesVal, objTypeVal, objDistVal, objSizeVal, objMagVal, objBvVal, objIllumVal;
    QString objSizeLabel, objMagLabel;

    switch (obj->type())
    {
        case SkyObject::STAR:
        {
            s = (StarObject *)obj;

            objNamesVal = s->longname();

            if (s->getHDIndex() != 0)
            {
                if (!s->longname().isEmpty())
                {
                    objNamesVal = s->longname() + QString(", HD%1").arg(QString::number(s->getHDIndex()));
                }

                else
                {
                    objNamesVal = QString(", HD%1").arg(QString::number(s->getHDIndex()));
                }
            }

            objTypeVal = s->sptype() + ' ' + i18n("star");
            objMagVal = i18nc("number in magnitudes", "%1 mag", QLocale().toString(s->mag(), 1)); //show to tenths place

            if (s->getBVIndex() < 30.0)
            {
                objBvVal = QString::number(s->getBVIndex(), 'g', 2);
            }

            //distance
            if (s->distance() > 2000. || s->distance() < 0.) // parallax < 0.5 mas
            {
                objDistVal = i18nc("larger than 2000 parsecs", "> 2000 pc");
            }

            else if (s->distance() > 50.0) //show to nearest integer
            {
                objDistVal = i18nc("number in parsecs", "%1 pc", QLocale().toString(s->distance(), 0));
            }

            else if (s->distance() > 10.0) //show to tenths place
            {
                objDistVal = i18nc("number in parsecs", "%1 pc", QLocale().toString(s->distance(), 1));
            }

            else //show to hundredths place
            {
                objDistVal = i18nc("number in parsecs", "%1 pc", QLocale().toString(s->distance(), 2));
            }

            //Note multiplicity/variability in angular size label
            if (s->isMultiple() && s->isVariable())
            {
                objSizeLabel = i18nc("the star is a multiple star", "multiple") + ',';
                objSizeVal   = i18nc("the star is a variable star", "variable");
            }

            else if (s->isMultiple())
            {
                objSizeLabel = i18nc("the star is a multiple star", "multiple");
            }

            else if (s->isVariable())
            {
                objSizeLabel = i18nc("the star is a variable star", "variable");
            }

            objIllumVal = "--";

            break; //End of stars case
        }

        case SkyObject::ASTEROID: //[fall through to planets]

        case SkyObject::COMET: //[fall through to planets]

        case SkyObject::MOON: //[fall through to planets]

        case SkyObject::PLANET:
        {
            ps = dynamic_cast<KSPlanetBase *>(obj);

            objNamesVal = ps->longname();
            //Type is "G5 star" for Sun
            if (ps->name() == i18n("Sun"))
            {
                objTypeVal = i18n("G5 star");
            }

            else if (ps->name() == i18n("Moon"))
            {
                objTypeVal = ps->translatedName();
            }

            else if (ps->name() == i18nc("Asteroid name (optional)", "Pluto") || ps->name() == i18nc("Asteroid name (optional)", "Ceres") ||
                     ps->name() == i18nc("Asteroid name (optional)", "Eris")) // TODO: Check if Ceres / Eris have translations and i18n() them
            {
                objTypeVal = i18n("Dwarf planet");
            }

            else
            {
                objTypeVal = ps->typeName();
            }

            //Magnitude: The moon displays illumination fraction instead
            if (obj->name() == i18n("Moon"))
            {
                KSMoon * const m = dynamic_cast<KSMoon *>(obj);
                if (m)
                    objIllumVal = QString("%1 %").arg(QLocale().toString(m->illum() * 100., 0));
            }

            objMagVal =
                i18nc("number in magnitudes", "%1 mag", QLocale().toString(ps->mag(), 1)); //show to tenths place

            //Distance from Earth.  The moon requires a unit conversion
            if (ps->name() == i18n("Moon"))
            {
                objDistVal = i18nc("distance in kilometers", "%1 km", QLocale().toString(ps->rearth() * AU_KM));
            }

            else
            {
                objDistVal = i18nc("distance in Astronomical Units", "%1 AU", QLocale().toString(ps->rearth()));
            }

            //Angular size; moon and sun in arcmin, others in arcsec
            if (ps->angSize())
            {
                if (ps->name() == i18n("Sun") || ps->name() == i18n("Moon"))
                {
                    // Needn't be a plural form because sun / moon will never contract to 1 arcminute
                    objSizeVal = i18nc("angular size in arcminutes", "%1 arcmin", QLocale().toString(ps->angSize()));
                }

                else
                {
                    objSizeVal =
                        i18nc("angular size in arcseconds", "%1 arcsec", QLocale().toString(ps->angSize() * 60.0));
                }
            }

            else
            {
                objSizeVal = "--";
            }

            break; //End of planets/comets/asteroids case
        }

        default: //Deep-sky objects
        {
            dso = (CatalogObject *)obj;

            //Show all names recorded for the object
            if (!dso->longname().isEmpty() && dso->longname() != dso->name())
            {
                pname = dso->translatedLongName();
                oname = dso->translatedName();
            }

            else
            {
                pname = dso->translatedName();
            }

            if (!dso->translatedName2().isEmpty())
            {
                if (oname.isEmpty())
                {
                    oname = dso->translatedName2();
                }

                else
                {
                    oname += ", " + dso->translatedName2();
                }
            }


            if (!oname.isEmpty())
            {
                pname += ", " + oname;
            }

            objNamesVal = pname;

            objTypeVal = dso->typeName();

            if (dso->type() == SkyObject::RADIO_SOURCE)
            {
                objMagLabel =
                    i18nc("integrated flux at a frequency", "Flux(%1):", CatalogsDB::flux_frequency);
                objMagVal = i18nc("integrated flux value", "%1 %2", QLocale().toString(dso->flux(), 1),
                                  CatalogsDB::flux_unit); //show to tenths place
            }

            if (dso->mag() > 90.0)
            {
                objMagVal = "--";
            }

            else
            {
                objMagVal =
                    i18nc("number in magnitudes", "%1 mag", QLocale().toString(dso->mag(), 1)); //show to tenths place
            }

            //No distances at this point...
            objDistVal = "--";

            //Only show decimal place for small angular sizes
            if (dso->a() > 10.0)
            {
                objSizeVal = i18nc("angular size in arcminutes", "%1 arcmin", QLocale().toString(dso->a(), 0));
            }

            else if (dso->a())
            {
                objSizeVal = i18nc("angular size in arcminutes", "%1 arcmin", QLocale().toString(dso->a(), 1));
            }

            else
            {
                objSizeVal = "--";
            }

            break; //End of deep-space objects case
        }
    }

    //Common to all types:
    if (obj->type() == SkyObject::CONSTELLATION)
    {
        objTypeVal = KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(obj);
    }

    else
    {
        objTypeVal =
            i18nc("%1 type of sky object (planet, asteroid etc), %2 name of a constellation", "%1 in %2", objTypeVal,
                  KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(obj));
    }

    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25);
    m_TableFormat.setColumnWidthConstraints(constraints);

    QTextTable *table = cursor.insertTable(5, 4, m_TableFormat);
    table->mergeCells(0, 0, 1, 4);
    QTextBlockFormat centered;
    centered.setAlignment(Qt::AlignCenter);
    table->cellAt(0, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(0, 0).firstCursorPosition().insertText(i18n("General"), m_TableTitleCharFormat);

    table->mergeCells(1, 1, 1, 3);
    table->cellAt(1, 0).firstCursorPosition().insertText(i18n("Names:"), m_ItemNameCharFormat);
    table->cellAt(1, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 1).firstCursorPosition().insertText(objNamesVal, m_ItemValueCharFormat);

    table->cellAt(2, 0).firstCursorPosition().insertText(i18n("Type:"), m_ItemNameCharFormat);
    table->cellAt(2, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 1).firstCursorPosition().insertText(objTypeVal, m_ItemValueCharFormat);

    table->cellAt(3, 0).firstCursorPosition().insertText(i18n("Distance:"), m_ItemNameCharFormat);
    table->cellAt(3, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(3, 1).firstCursorPosition().insertText(objDistVal, m_ItemValueCharFormat);

    table->cellAt(4, 0).firstCursorPosition().insertText(i18n("Size:"), m_ItemNameCharFormat);
    table->cellAt(4, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(4, 1).firstCursorPosition().insertText(objSizeVal, m_ItemValueCharFormat);

    table->cellAt(2, 2).firstCursorPosition().insertText(i18n("Magnitude:"), m_ItemNameCharFormat);
    table->cellAt(2, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 3).firstCursorPosition().insertText(objMagVal, m_ItemValueCharFormat);

    table->cellAt(3, 2).firstCursorPosition().insertText(i18n("B-V index:"), m_ItemNameCharFormat);
    table->cellAt(3, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(3, 3).firstCursorPosition().insertText(objBvVal, m_ItemValueCharFormat);

    table->cellAt(4, 2).firstCursorPosition().insertText(i18n("Illumination:"), m_ItemNameCharFormat);
    table->cellAt(4, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(4, 3).firstCursorPosition().insertText(objIllumVal, m_ItemValueCharFormat);
}

void DetailsTable::createAsteroidCometTable(SkyObject *obj)
{
    clearContents();

    QTextCursor cursor = m_Document->rootFrame()->firstCursorPosition();

    QString perihelionVal, orbitIdVal, neoVal, diamVal, rotPeriodVal, moidVal;
    QString orbitClassVal, albedoVal, dimVal, periodVal;

    // Add specifics data
    switch (obj->type())
    {
        case SkyObject::ASTEROID:
        {
            KSAsteroid *ast = (KSAsteroid *)obj;

            // Perihelion
            perihelionVal = QString::number(ast->getPerihelion()) + " AU";

            // Earth MOID
            moidVal = ast->getEarthMOID() == 0 ? QString("--") : QString::number(ast->getEarthMOID()) + QString(" AU");

            // Orbit ID
            orbitIdVal = ast->getOrbitID();

            // Orbit Class
            orbitClassVal = ast->getOrbitClass();

            // NEO
            neoVal = ast->isNEO() ? i18n("Yes") : i18n("No");

            // Albedo
            albedoVal = ast->getAlbedo() == 0 ? QString("--") : QString::number(ast->getAlbedo());

            // Diameter
            diamVal = ast->getDiameter() == 0 ? QString("--") : QString::number(ast->getDiameter()) + QString(" km");

            // Dimensions
            dimVal = ast->getDimensions().isEmpty() ? QString("--") : ast->getDimensions() + QString(" km");

            // Rotation period
            rotPeriodVal = ast->getRotationPeriod() == 0 ? QString("--") :
                           QString::number(ast->getRotationPeriod()) + QString(" h");

            // Period
            periodVal = ast->getPeriod() == 0 ? QString("--") : QString::number(ast->getPeriod()) + QString(" y");

            break;
        }

        case SkyObject::COMET:
        {
            KSComet *com = (KSComet *)obj;

            // Perihelion
            perihelionVal = QString::number(com->getPerihelion()) + " AU";

            // Earth MOID
            moidVal = com->getEarthMOID() == 0 ? QString("--") : QString::number(com->getEarthMOID()) + QString(" AU");

            // Orbit ID
            orbitIdVal = com->getOrbitID();

            // Orbit Class
            orbitClassVal = com->getOrbitClass();

            // NEO
            neoVal = com->isNEO() ? i18n("Yes") : i18n("No");

            // Albedo
            albedoVal = com->getAlbedo() == 0 ? QString("--") : QString::number(com->getAlbedo());

            // Diameter
            diamVal = com->getDiameter() == 0 ? QString("--") : QString::number(com->getDiameter()) + QString(" km");

            // Dimensions
            dimVal = com->getDimensions().isEmpty() ? QString("--") : com->getDimensions() + QString(" km");

            // Rotation period
            rotPeriodVal = com->getRotationPeriod() == 0 ? QString("--") :
                           QString::number(com->getRotationPeriod()) + QString(" h");

            // Period
            periodVal = com->getPeriod() == 0 ? QString("--") : QString::number(com->getPeriod()) + QString(" y");

            break;
        }

        default:
        {
            return;
        }
    }

    // Set column width constraints
    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25);
    m_TableFormat.setColumnWidthConstraints(constraints);

    QTextTable *table = cursor.insertTable(6, 4, m_TableFormat);
    table->mergeCells(0, 0, 1, 4);
    QTextBlockFormat centered;
    centered.setAlignment(Qt::AlignCenter);
    table->cellAt(0, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(0, 0).firstCursorPosition().insertText(i18n("Asteroid/Comet details"), m_TableTitleCharFormat);

    table->cellAt(1, 0).firstCursorPosition().insertText(i18n("Perihelion:"), m_ItemNameCharFormat);
    table->cellAt(1, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 1).firstCursorPosition().insertText(perihelionVal, m_ItemValueCharFormat);

    table->cellAt(2, 0).firstCursorPosition().insertText(i18n("Orbit ID:"), m_ItemNameCharFormat);
    table->cellAt(2, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 1).firstCursorPosition().insertText(orbitIdVal, m_ItemValueCharFormat);

    table->cellAt(3, 0).firstCursorPosition().insertText(i18n("NEO:"), m_ItemNameCharFormat);
    table->cellAt(3, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(3, 1).firstCursorPosition().insertText(neoVal, m_ItemValueCharFormat);

    table->cellAt(4, 0).firstCursorPosition().insertText(i18n("Diameter:"), m_ItemNameCharFormat);
    table->cellAt(4, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(4, 1).firstCursorPosition().insertText(diamVal, m_ItemValueCharFormat);

    table->cellAt(5, 0).firstCursorPosition().insertText(i18n("Rotation period:"), m_ItemNameCharFormat);
    table->cellAt(5, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(5, 1).firstCursorPosition().insertText(rotPeriodVal, m_ItemValueCharFormat);

    table->cellAt(1, 2).firstCursorPosition().insertText(i18n("Earth MOID:"), m_ItemNameCharFormat);
    table->cellAt(1, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 3).firstCursorPosition().insertText(moidVal, m_ItemValueCharFormat);

    table->cellAt(2, 2).firstCursorPosition().insertText(i18n("Orbit class:"), m_ItemNameCharFormat);
    table->cellAt(2, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 3).firstCursorPosition().insertText(orbitClassVal, m_ItemValueCharFormat);

    table->cellAt(3, 2).firstCursorPosition().insertText(i18n("Albedo:"), m_ItemNameCharFormat);
    table->cellAt(3, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(3, 3).firstCursorPosition().insertText(albedoVal, m_ItemValueCharFormat);

    table->cellAt(4, 2).firstCursorPosition().insertText(i18n("Dimensions:"), m_ItemNameCharFormat);
    table->cellAt(4, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(4, 3).firstCursorPosition().insertText(dimVal, m_ItemValueCharFormat);

    table->cellAt(5, 2).firstCursorPosition().insertText(i18n("Period:"), m_ItemNameCharFormat);
    table->cellAt(5, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(5, 3).firstCursorPosition().insertText(periodVal, m_ItemValueCharFormat);
}

void DetailsTable::createCoordinatesTable(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo)
{
    clearContents();

    QTextCursor cursor = m_Document->rootFrame()->firstCursorPosition();

    // Set column width constraints
    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25);
    m_TableFormat.setColumnWidthConstraints(constraints);

    // Insert table & row containing table name
    QTextTable *table = cursor.insertTable(4, 4, m_TableFormat);
    table->mergeCells(0, 0, 1, 4);
    QTextBlockFormat centered;
    centered.setAlignment(Qt::AlignCenter);
    table->cellAt(0, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(0, 0).firstCursorPosition().insertText(i18n("Coordinates"), m_TableTitleCharFormat);

    //Coordinates Section:
    //Don't use KLocale::formatNumber() for the epoch string,
    //because we don't want a thousands-place separator!
    QString sEpoch = QString::number(ut.epoch(), 'f', 1);
    //Replace the decimal point with localized decimal symbol
    sEpoch.replace('.', QLocale().decimalPoint());

    table->cellAt(1, 0).firstCursorPosition().insertText(i18n("RA (%1):", sEpoch), m_ItemNameCharFormat);
    table->cellAt(1, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 1).firstCursorPosition().insertText(obj->ra().toHMSString(), m_ItemValueCharFormat);

    table->cellAt(2, 0).firstCursorPosition().insertText(i18n("Dec (%1):", sEpoch), m_ItemNameCharFormat);
    table->cellAt(2, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 1).firstCursorPosition().insertText(obj->dec().toDMSString(), m_ItemValueCharFormat);

    table->cellAt(3, 0).firstCursorPosition().insertText(i18n("Hour angle:"), m_ItemNameCharFormat);
    table->cellAt(3, 0).firstCursorPosition().setBlockFormat(centered);
    //Hour Angle can be negative, but dms HMS expressions cannot.
    //Here's a kludgy workaround:
    dms lst = geo->GSTtoLST(ut.gst());
    dms ha(lst.Degrees() - obj->ra().Degrees());
    QChar sgn('+');
    if (ha.Hours() > 12.0)
    {
        ha.setH(24.0 - ha.Hours());
        sgn = '-';
    }
    table->cellAt(3, 1).firstCursorPosition().insertText(QString("%1%2").arg(sgn).arg(ha.toHMSString()),
            m_ItemValueCharFormat);

    table->cellAt(1, 2).firstCursorPosition().insertText(i18n("Azimuth:"), m_ItemNameCharFormat);
    table->cellAt(1, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 3).firstCursorPosition().insertText(obj->az().toDMSString(), m_ItemValueCharFormat);

    table->cellAt(2, 2).firstCursorPosition().insertText(i18n("Altitude:"), m_ItemNameCharFormat);
    table->cellAt(2, 2).firstCursorPosition().setBlockFormat(centered);
    dms a;
    if (Options::useAltAz())
    {
        a = obj->alt();
    }

    else
    {
        a = obj->altRefracted();
    }
    table->cellAt(2, 3).firstCursorPosition().insertText(a.toDMSString(), m_ItemValueCharFormat);

    table->cellAt(3, 2).firstCursorPosition().insertText(i18n("Airmass:"), m_ItemNameCharFormat);
    table->cellAt(3, 2).firstCursorPosition().setBlockFormat(centered);
    //Airmass is approximated as the secant of the zenith distance,
    //equivalent to 1./sin(Alt).  Beware of Inf at Alt=0!
    QString aMassStr;
    if (obj->alt().Degrees() > 0.0)
    {
        aMassStr = QLocale().toString(1. / sin(obj->alt().radians()), 2);
    }

    else
    {
        aMassStr = "--";
    }
    table->cellAt(3, 3).firstCursorPosition().insertText(aMassStr, m_ItemValueCharFormat);

    // Restore the position and other time-dependent parameters
    obj->recomputeCoords(ut, geo);
}

void DetailsTable::createRSTTAble(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo)
{
    clearContents();

    QTextCursor cursor = m_Document->rootFrame()->firstCursorPosition();

    QString rtValue, stValue;   // Rise/Set time values
    QString azRValue, azSValue; // Rise/Set azimuth values

    //Prepare time/position variables
    QTime rt = obj->riseSetTime(ut, geo, true);   //true = use rise time
    dms raz  = obj->riseSetTimeAz(ut, geo, true); //true = use rise time

    //If transit time is before rise time, use transit time for tomorrow
    QTime tt = obj->transitTime(ut, geo);
    dms talt = obj->transitAltitude(ut, geo);
    if (tt < rt)
    {
        tt   = obj->transitTime(ut.addDays(1), geo);
        talt = obj->transitAltitude(ut.addDays(1), geo);
    }

    //If set time is before rise time, use set time for tomorrow
    QTime st = obj->riseSetTime(ut, geo, false);   //false = use set time
    dms saz  = obj->riseSetTimeAz(ut, geo, false); //false = use set time
    if (st < rt)
    {
        st  = obj->riseSetTime(ut.addDays(1), geo, false);   //false = use set time
        saz = obj->riseSetTimeAz(ut.addDays(1), geo, false); //false = use set time
    }

    if (rt.isValid())
    {
        rtValue  = QString::asprintf("%02d:%02d", rt.hour(), rt.minute());
        stValue  = QString::asprintf("%02d:%02d", st.hour(), st.minute());
        azRValue = raz.toDMSString();
        azSValue = saz.toDMSString();
    }

    else
    {
        if (obj->alt().Degrees() > 0.0)
        {
            rtValue = i18n("Circumpolar");
            stValue = i18n("Circumpolar");
        }

        else
        {
            rtValue = i18n("Never rises");
            stValue = i18n("Never rises");
        }

        azRValue = i18nc("Not Applicable", "N/A");
        azSValue = i18nc("Not Applicable", "N/A");
    }

    // Set column width constraints
    QVector<QTextLength> constraints;
    constraints << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25)
                << QTextLength(QTextLength::PercentageLength, 25) << QTextLength(QTextLength::PercentageLength, 25);
    m_TableFormat.setColumnWidthConstraints(constraints);

    // Insert table & row containing table name
    QTextTable *table = cursor.insertTable(4, 4, m_TableFormat);
    table->mergeCells(0, 0, 1, 4);
    QTextBlockFormat centered;
    centered.setAlignment(Qt::AlignCenter);
    table->cellAt(0, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(0, 0).firstCursorPosition().insertText(i18n("Rise/Set/Transit"), m_TableTitleCharFormat);

    // Insert cell names & values
    table->cellAt(1, 0).firstCursorPosition().insertText(i18n("Rise time:"), m_ItemNameCharFormat);
    table->cellAt(1, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 1).firstCursorPosition().insertText(rtValue, m_ItemValueCharFormat);

    table->cellAt(2, 0).firstCursorPosition().insertText(i18n("Transit time:"), m_ItemNameCharFormat);
    table->cellAt(2, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 1).firstCursorPosition().insertText(QString::asprintf("%02d:%02d", tt.hour(), tt.minute()),
            m_ItemValueCharFormat);

    table->cellAt(3, 0).firstCursorPosition().insertText(i18n("Set time:"), m_ItemNameCharFormat);
    table->cellAt(3, 0).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(3, 1).firstCursorPosition().insertText(stValue, m_ItemValueCharFormat);

    table->cellAt(1, 2).firstCursorPosition().insertText(i18n("Azimuth at rise:"), m_ItemNameCharFormat);
    table->cellAt(1, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(1, 3).firstCursorPosition().insertText(azRValue, m_ItemValueCharFormat);

    table->cellAt(2, 2).firstCursorPosition().insertText(i18n("Altitude at transit:"), m_ItemNameCharFormat);
    table->cellAt(2, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(2, 3).firstCursorPosition().insertText(talt.toDMSString(), m_ItemValueCharFormat);

    table->cellAt(3, 2).firstCursorPosition().insertText(i18n("Azimuth at set:"), m_ItemNameCharFormat);
    table->cellAt(3, 2).firstCursorPosition().setBlockFormat(centered);
    table->cellAt(3, 3).firstCursorPosition().insertText(azSValue, m_ItemValueCharFormat);

    // Restore the position and other time-dependent parameters
    obj->recomputeCoords(ut, geo);
}

void DetailsTable::clearContents()
{
    m_Document->clear();
}

void DetailsTable::setDefaultFormatting()
{
    // Set default table format
    m_TableFormat.setAlignment(Qt::AlignCenter);
    m_TableFormat.setBorder(4);
    m_TableFormat.setCellPadding(2);
    m_TableFormat.setCellSpacing(2);

    // Set default table title character format
    m_TableTitleCharFormat.setFont(QFont("Times", 12, QFont::Bold));
    m_TableTitleCharFormat.setFontCapitalization(QFont::Capitalize);

    // Set default table item name character format
    m_ItemNameCharFormat.setFont(QFont("Times", 10, QFont::Bold));

    // Set default table item value character format
    m_ItemValueCharFormat.setFont(QFont("Times", 10));
}
