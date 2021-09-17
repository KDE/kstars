/*
    SPDX-FileCopyrightText: 2002 Mark Hollomon <mhh@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksutils.h"
#include "config-kstars.h"
#include "ksnotification.h"
#include "kstars_debug.h"

#include "catalogobject.h"
#ifndef KSTARS_LITE
#include "kswizard.h"
#endif
#include "Options.h"
#include "starobject.h"
#include "auxiliary/kspaths.h"

#ifndef KSTARS_LITE
#include <KMessageBox>
#endif

#ifdef HAVE_LIBRAW
#include <libraw/libraw.h>
#endif

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include "windows.h"
#else //Linux
#include <QProcess>
#endif

#include <QPointer>
#include <QProcessEnvironment>
#include <QLoggingCategory>

#ifdef HAVE_STELLARSOLVER
#include <stellarsolver.h>
#endif

namespace KSUtils
{

bool isHardwareLimited()
{
#ifdef __arm__
    return true;
#else
    return false;
#endif
}

bool openDataFile(QFile &file, const QString &s)
{
    QString FileName = KSPaths::locate(QStandardPaths::AppDataLocation, s);
    if (!FileName.isNull())
    {
        file.setFileName(FileName);
        return file.open(QIODevice::ReadOnly);
    }
    return false;
}

QString getDSSURL(const SkyPoint *const p)
{
    double height, width;
    double dss_default_size = Options::defaultDSSImageSize();
    double dss_padding      = Options::dSSPadding();

    Q_ASSERT(p);
    Q_ASSERT(dss_default_size > 0.0 && dss_padding >= 0.0);

    const auto *dso = dynamic_cast<const CatalogObject *>(p);

    // Decide what to do about the height and width
    if (dso)
    {
        // For deep-sky objects, use their height and width information
        double a, b, pa;
        a = dso->a();
        b = dso->a() *
            dso->e(); // Use a * e instead of b, since e() returns 1 whenever one of the dimensions is zero. This is important for circular objects
        pa = dso->pa() * dms::DegToRad;

        // We now want to convert a, b, and pa into an image
        // height and width -- i.e. a dRA and dDec.
        // DSS uses dDec for height and dRA for width. (i.e. "top" is north in the DSS images, AFAICT)
        // From some trigonometry, assuming we have a rectangular object (worst case), we need:
        width  = a * sin(pa) + b * cos(pa);
        height = a * cos(pa) + b * sin(pa);
        // 'a' and 'b' are in arcminutes, so height and width are in arcminutes

        // Pad the RA and Dec, so that we show more of the sky than just the object.
        height += dss_padding;
        width += dss_padding;
    }
    else
    {
        // For a generic sky object, we don't know what to do. So
        // we just assume the default size.
        height = width = dss_default_size;
    }
    // There's no point in tiny DSS images that are smaller than dss_default_size
    if (height < dss_default_size)
        height = dss_default_size;
    if (width < dss_default_size)
        width = dss_default_size;

    return getDSSURL(p->ra0(), p->dec0(), width, height);
}

QString getDSSURL(const dms &ra, const dms &dec, float width, float height, const QString &type)
{
    const QString URLprefix("https://archive.stsci.edu/cgi-bin/dss_search?");
    QString URLsuffix             = QString("&e=J2000&f=%1&c=none&fov=NONE").arg(type);
    const double dss_default_size = Options::defaultDSSImageSize();

    char decsgn = (dec.Degrees() < 0.0) ? '-' : '+';
    int dd      = abs(dec.degree());
    int dm      = abs(dec.arcmin());
    int ds      = abs(dec.arcsec());

    // Infinite and NaN sizes are replaced by the default size and tiny DSS images are resized to default size
    if (!qIsFinite(height) || height <= 0.0)
        height = dss_default_size;
    if (!qIsFinite(width) || width <= 0.0)
        width = dss_default_size;

    // DSS accepts images that are no larger than 75 arcminutes
    if (height > 75.0)
        height = 75.0;
    if (width > 75.0)
        width = 75.0;

    QString RAString, DecString, SizeString;
    DecString  = QString::asprintf("&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds);
    RAString   = QString::asprintf("r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second());
    SizeString = QString::asprintf("&h=%02.1f&w=%02.1f", height, width);

    return (URLprefix + RAString + DecString + SizeString + URLsuffix);
}

QString toDirectionString(dms angle)
{
    // TODO: Instead of doing it this way, it would be nicer to
    // compute the string to arbitrary precision. Although that will
    // not be easy to localize.  (Consider, for instance, Indian
    // languages that have special names for the intercardinal points)
    // -- asimha

    static const char *directions[] = { I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "N"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "NNE"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "NE"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "ENE"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "E"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "ESE"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "SE"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "SSE"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "S"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "SSW"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "SW"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "WSW"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "W"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "WNW"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "NW"),
                                        I18N_NOOP2("Abbreviated cardinal / intercardinal etc. direction", "NNW"),
                                        I18N_NOOP2("Unknown cardinal / intercardinal direction", "???")
                                      };

    int index = (int)((angle.reduce().Degrees() + 11.25) / 22.5); // A number between 0 and 16 (inclusive) is expected

    if (index < 0 || index > 16)
        index = 16; // Something went wrong.
    else
        index = (index == 16 ? 0 : index);

    return i18nc("Abbreviated cardinal / intercardinal etc. direction", directions[index]);
}

QList<SkyObject *> *castStarObjListToSkyObjList(QList<StarObject *> *starObjList)
{
    QList<SkyObject *> *skyObjList = new QList<SkyObject *>();

    foreach (StarObject *so, *starObjList)
    {
        skyObjList->append(so);
    }
    return skyObjList;
}

QString constGenetiveFromAbbrev(const QString &code)
{
    if (code == "And")
        return QString("Andromedae");
    if (code == "Ant")
        return QString("Antliae");
    if (code == "Aps")
        return QString("Apodis");
    if (code == "Aqr")
        return QString("Aquarii");
    if (code == "Aql")
        return QString("Aquilae");
    if (code == "Ara")
        return QString("Arae");
    if (code == "Ari")
        return QString("Arietis");
    if (code == "Aur")
        return QString("Aurigae");
    if (code == "Boo")
        return QString("Bootis");
    if (code == "Cae")
        return QString("Caeli");
    if (code == "Cam")
        return QString("Camelopardalis");
    if (code == "Cnc")
        return QString("Cancri");
    if (code == "CVn")
        return QString("Canum Venaticorum");
    if (code == "CMa")
        return QString("Canis Majoris");
    if (code == "CMi")
        return QString("Canis Minoris");
    if (code == "Cap")
        return QString("Capricorni");
    if (code == "Car")
        return QString("Carinae");
    if (code == "Cas")
        return QString("Cassiopeiae");
    if (code == "Cen")
        return QString("Centauri");
    if (code == "Cep")
        return QString("Cephei");
    if (code == "Cet")
        return QString("Ceti");
    if (code == "Cha")
        return QString("Chamaeleontis");
    if (code == "Cir")
        return QString("Circini");
    if (code == "Col")
        return QString("Columbae");
    if (code == "Com")
        return QString("Comae Berenices");
    if (code == "CrA")
        return QString("Coronae Austrinae");
    if (code == "CrB")
        return QString("Coronae Borealis");
    if (code == "Crv")
        return QString("Corvi");
    if (code == "Crt")
        return QString("Crateris");
    if (code == "Cru")
        return QString("Crucis");
    if (code == "Cyg")
        return QString("Cygni");
    if (code == "Del")
        return QString("Delphini");
    if (code == "Dor")
        return QString("Doradus");
    if (code == "Dra")
        return QString("Draconis");
    if (code == "Equ")
        return QString("Equulei");
    if (code == "Eri")
        return QString("Eridani");
    if (code == "For")
        return QString("Fornacis");
    if (code == "Gem")
        return QString("Geminorum");
    if (code == "Gru")
        return QString("Gruis");
    if (code == "Her")
        return QString("Herculis");
    if (code == "Hor")
        return QString("Horologii");
    if (code == "Hya")
        return QString("Hydrae");
    if (code == "Hyi")
        return QString("Hydri");
    if (code == "Ind")
        return QString("Indi");
    if (code == "Lac")
        return QString("Lacertae");
    if (code == "Leo")
        return QString("Leonis");
    if (code == "LMi")
        return QString("Leonis Minoris");
    if (code == "Lep")
        return QString("Leporis");
    if (code == "Lib")
        return QString("Librae");
    if (code == "Lup")
        return QString("Lupi");
    if (code == "Lyn")
        return QString("Lyncis");
    if (code == "Lyr")
        return QString("Lyrae");
    if (code == "Men")
        return QString("Mensae");
    if (code == "Mic")
        return QString("Microscopii");
    if (code == "Mon")
        return QString("Monocerotis");
    if (code == "Mus")
        return QString("Muscae");
    if (code == "Nor")
        return QString("Normae");
    if (code == "Oct")
        return QString("Octantis");
    if (code == "Oph")
        return QString("Ophiuchi");
    if (code == "Ori")
        return QString("Orionis");
    if (code == "Pav")
        return QString("Pavonis");
    if (code == "Peg")
        return QString("Pegasi");
    if (code == "Per")
        return QString("Persei");
    if (code == "Phe")
        return QString("Phoenicis");
    if (code == "Pic")
        return QString("Pictoris");
    if (code == "Psc")
        return QString("Piscium");
    if (code == "PsA")
        return QString("Piscis Austrini");
    if (code == "Pup")
        return QString("Puppis");
    if (code == "Pyx")
        return QString("Pyxidis");
    if (code == "Ret")
        return QString("Reticuli");
    if (code == "Sge")
        return QString("Sagittae");
    if (code == "Sgr")
        return QString("Sagittarii");
    if (code == "Sco")
        return QString("Scorpii");
    if (code == "Scl")
        return QString("Sculptoris");
    if (code == "Sct")
        return QString("Scuti");
    if (code == "Ser")
        return QString("Serpentis");
    if (code == "Sex")
        return QString("Sextantis");
    if (code == "Tau")
        return QString("Tauri");
    if (code == "Tel")
        return QString("Telescopii");
    if (code == "Tri")
        return QString("Trianguli");
    if (code == "TrA")
        return QString("Trianguli Australis");
    if (code == "Tuc")
        return QString("Tucanae");
    if (code == "UMa")
        return QString("Ursae Majoris");
    if (code == "UMi")
        return QString("Ursae Minoris");
    if (code == "Vel")
        return QString("Velorum");
    if (code == "Vir")
        return QString("Virginis");
    if (code == "Vol")
        return QString("Volantis");
    if (code == "Vul")
        return QString("Vulpeculae");
    return code;
}

QString constNameFromAbbrev(const QString &code)
{
    if (code == "And")
        return QString("Andromeda");
    if (code == "Ant")
        return QString("Antlia");
    if (code == "Aps")
        return QString("Apus");
    if (code == "Aqr")
        return QString("Aquarius");
    if (code == "Aql")
        return QString("Aquila");
    if (code == "Ara")
        return QString("Ara");
    if (code == "Ari")
        return QString("Aries");
    if (code == "Aur")
        return QString("Auriga");
    if (code == "Boo")
        return QString("Bootes");
    if (code == "Cae")
        return QString("Caelum");
    if (code == "Cam")
        return QString("Camelopardalis");
    if (code == "Cnc")
        return QString("Cancer");
    if (code == "CVn")
        return QString("Canes Venatici");
    if (code == "CMa")
        return QString("Canis Major");
    if (code == "CMi")
        return QString("Canis Minor");
    if (code == "Cap")
        return QString("Capricornus");
    if (code == "Car")
        return QString("Carina");
    if (code == "Cas")
        return QString("Cassiopeia");
    if (code == "Cen")
        return QString("Centaurus");
    if (code == "Cep")
        return QString("Cepheus");
    if (code == "Cet")
        return QString("Cetus");
    if (code == "Cha")
        return QString("Chamaeleon");
    if (code == "Cir")
        return QString("Circinus");
    if (code == "Col")
        return QString("Columba");
    if (code == "Com")
        return QString("Coma Berenices");
    if (code == "CrA")
        return QString("Corona Australis");
    if (code == "CrB")
        return QString("Corona Borealis");
    if (code == "Crv")
        return QString("Corvus");
    if (code == "Crt")
        return QString("Crater");
    if (code == "Cru")
        return QString("Crux");
    if (code == "Cyg")
        return QString("Cygnus");
    if (code == "Del")
        return QString("Delphinus");
    if (code == "Dor")
        return QString("Doradus");
    if (code == "Dra")
        return QString("Draco");
    if (code == "Equ")
        return QString("Equuleus");
    if (code == "Eri")
        return QString("Eridanus");
    if (code == "For")
        return QString("Fornax");
    if (code == "Gem")
        return QString("Gemini");
    if (code == "Gru")
        return QString("Grus");
    if (code == "Her")
        return QString("Hercules");
    if (code == "Hor")
        return QString("Horologium");
    if (code == "Hya")
        return QString("Hydra");
    if (code == "Hyi")
        return QString("Hydrus");
    if (code == "Ind")
        return QString("Indus");
    if (code == "Lac")
        return QString("Lacerta");
    if (code == "Leo")
        return QString("Leo");
    if (code == "LMi")
        return QString("Leo Minor");
    if (code == "Lep")
        return QString("Lepus");
    if (code == "Lib")
        return QString("Libra");
    if (code == "Lup")
        return QString("Lupus");
    if (code == "Lyn")
        return QString("Lynx");
    if (code == "Lyr")
        return QString("Lyra");
    if (code == "Men")
        return QString("Mensa");
    if (code == "Mic")
        return QString("Microscopium");
    if (code == "Mon")
        return QString("Monoceros");
    if (code == "Mus")
        return QString("Musca");
    if (code == "Nor")
        return QString("Norma");
    if (code == "Oct")
        return QString("Octans");
    if (code == "Oph")
        return QString("Ophiuchus");
    if (code == "Ori")
        return QString("Orion");
    if (code == "Pav")
        return QString("Pavo");
    if (code == "Peg")
        return QString("Pegasus");
    if (code == "Per")
        return QString("Perseus");
    if (code == "Phe")
        return QString("Phoenix");
    if (code == "Pic")
        return QString("Pictor");
    if (code == "Psc")
        return QString("Pisces");
    if (code == "PsA")
        return QString("Piscis Austrinus");
    if (code == "Pup")
        return QString("Puppis");
    if (code == "Pyx")
        return QString("Pyxis");
    if (code == "Ret")
        return QString("Reticulum");
    if (code == "Sge")
        return QString("Sagitta");
    if (code == "Sgr")
        return QString("Sagittarius");
    if (code == "Sco")
        return QString("Scorpius");
    if (code == "Scl")
        return QString("Sculptor");
    if (code == "Sct")
        return QString("Scutum");
    if (code == "Ser")
        return QString("Serpens");
    if (code == "Sex")
        return QString("Sextans");
    if (code == "Tau")
        return QString("Taurus");
    if (code == "Tel")
        return QString("Telescopium");
    if (code == "Tri")
        return QString("Triangulum");
    if (code == "TrA")
        return QString("Triangulum Australe");
    if (code == "Tuc")
        return QString("Tucana");
    if (code == "UMa")
        return QString("Ursa Major");
    if (code == "UMi")
        return QString("Ursa Minor");
    if (code == "Vel")
        return QString("Vela");
    if (code == "Vir")
        return QString("Virgo");
    if (code == "Vol")
        return QString("Volans");
    if (code == "Vul")
        return QString("Vulpecula");
    return code;
}

QString constNameToAbbrev(const QString &fullName_)
{
    QString fullName = fullName_.toLower();
    if (fullName == "andromeda")
        return QString("And");
    if (fullName == "antlia")
        return QString("Ant");
    if (fullName == "apus")
        return QString("Aps");
    if (fullName == "aquarius")
        return QString("Aqr");
    if (fullName == "aquila")
        return QString("Aql");
    if (fullName == "ara")
        return QString("Ara");
    if (fullName == "aries")
        return QString("Ari");
    if (fullName == "auriga")
        return QString("Aur");
    if (fullName == "bootes")
        return QString("Boo");
    if (fullName == "caelum")
        return QString("Cae");
    if (fullName == "camelopardalis")
        return QString("Cam");
    if (fullName == "cancer")
        return QString("Cnc");
    if (fullName == "canes venatici")
        return QString("CVn");
    if (fullName == "canis major")
        return QString("CMa");
    if (fullName == "canis minor")
        return QString("CMi");
    if (fullName == "capricornus")
        return QString("Cap");
    if (fullName == "carina")
        return QString("Car");
    if (fullName == "cassiopeia")
        return QString("Cas");
    if (fullName == "centaurus")
        return QString("Cen");
    if (fullName == "cepheus")
        return QString("Cep");
    if (fullName == "cetus")
        return QString("Cet");
    if (fullName == "chamaeleon")
        return QString("Cha");
    if (fullName == "circinus")
        return QString("Cir");
    if (fullName == "columba")
        return QString("Col");
    if (fullName == "coma berenices")
        return QString("Com");
    if (fullName == "corona australis")
        return QString("CrA");
    if (fullName == "corona borealis")
        return QString("CrB");
    if (fullName == "corvus")
        return QString("Crv");
    if (fullName == "crater")
        return QString("Crt");
    if (fullName == "crux")
        return QString("Cru");
    if (fullName == "cygnus")
        return QString("Cyg");
    if (fullName == "delphinus")
        return QString("Del");
    if (fullName == "doradus")
        return QString("Dor");
    if (fullName == "draco")
        return QString("Dra");
    if (fullName == "equuleus")
        return QString("Equ");
    if (fullName == "eridanus")
        return QString("Eri");
    if (fullName == "fornax")
        return QString("For");
    if (fullName == "gemini")
        return QString("Gem");
    if (fullName == "grus")
        return QString("Gru");
    if (fullName == "hercules")
        return QString("Her");
    if (fullName == "horologium")
        return QString("Hor");
    if (fullName == "hydra")
        return QString("Hya");
    if (fullName == "hydrus")
        return QString("Hyi");
    if (fullName == "indus")
        return QString("Ind");
    if (fullName == "lacerta")
        return QString("Lac");
    if (fullName == "leo")
        return QString("Leo");
    if (fullName == "leo minor")
        return QString("LMi");
    if (fullName == "lepus")
        return QString("Lep");
    if (fullName == "libra")
        return QString("Lib");
    if (fullName == "lupus")
        return QString("Lup");
    if (fullName == "lynx")
        return QString("Lyn");
    if (fullName == "lyra")
        return QString("Lyr");
    if (fullName == "mensa")
        return QString("Men");
    if (fullName == "microscopium")
        return QString("Mic");
    if (fullName == "monoceros")
        return QString("Mon");
    if (fullName == "musca")
        return QString("Mus");
    if (fullName == "norma")
        return QString("Nor");
    if (fullName == "octans")
        return QString("Oct");
    if (fullName == "ophiuchus")
        return QString("Oph");
    if (fullName == "orion")
        return QString("Ori");
    if (fullName == "pavo")
        return QString("Pav");
    if (fullName == "pegasus")
        return QString("Peg");
    if (fullName == "perseus")
        return QString("Per");
    if (fullName == "phoenix")
        return QString("Phe");
    if (fullName == "pictor")
        return QString("Pic");
    if (fullName == "pisces")
        return QString("Psc");
    if (fullName == "piscis austrinus")
        return QString("PsA");
    if (fullName == "puppis")
        return QString("Pup");
    if (fullName == "pyxis")
        return QString("Pyx");
    if (fullName == "reticulum")
        return QString("Ret");
    if (fullName == "sagitta")
        return QString("Sge");
    if (fullName == "sagittarius")
        return QString("Sgr");
    if (fullName == "scorpius")
        return QString("Sco");
    if (fullName == "sculptor")
        return QString("Scl");
    if (fullName == "scutum")
        return QString("Sct");
    if (fullName == "serpens")
        return QString("Ser");
    if (fullName == "sextans")
        return QString("Sex");
    if (fullName == "taurus")
        return QString("Tau");
    if (fullName == "telescopium")
        return QString("Tel");
    if (fullName == "triangulum")
        return QString("Tri");
    if (fullName == "triangulum australe")
        return QString("TrA");
    if (fullName == "tucana")
        return QString("Tuc");
    if (fullName == "ursa major")
        return QString("UMa");
    if (fullName == "ursa minor")
        return QString("UMi");
    if (fullName == "vela")
        return QString("Vel");
    if (fullName == "virgo")
        return QString("Vir");
    if (fullName == "volans")
        return QString("Vol");
    if (fullName == "vulpecula")
        return QString("Vul");
    return fullName_;
}

QString constGenetiveToAbbrev(const QString &genetive_)
{
    QString genetive = genetive_.toLower();
    if (genetive == "andromedae")
        return QString("And");
    if (genetive == "antliae")
        return QString("Ant");
    if (genetive == "apodis")
        return QString("Aps");
    if (genetive == "aquarii")
        return QString("Aqr");
    if (genetive == "aquilae")
        return QString("Aql");
    if (genetive == "arae")
        return QString("Ara");
    if (genetive == "arietis")
        return QString("Ari");
    if (genetive == "aurigae")
        return QString("Aur");
    if (genetive == "bootis")
        return QString("Boo");
    if (genetive == "caeli")
        return QString("Cae");
    if (genetive == "camelopardalis")
        return QString("Cam");
    if (genetive == "cancri")
        return QString("Cnc");
    if (genetive == "canum venaticorum")
        return QString("CVn");
    if (genetive == "canis majoris")
        return QString("CMa");
    if (genetive == "canis minoris")
        return QString("CMi");
    if (genetive == "capricorni")
        return QString("Cap");
    if (genetive == "carinae")
        return QString("Car");
    if (genetive == "cassiopeiae")
        return QString("Cas");
    if (genetive == "centauri")
        return QString("Cen");
    if (genetive == "cephei")
        return QString("Cep");
    if (genetive == "ceti")
        return QString("Cet");
    if (genetive == "chamaeleontis")
        return QString("Cha");
    if (genetive == "circini")
        return QString("Cir");
    if (genetive == "columbae")
        return QString("Col");
    if (genetive == "comae berenices")
        return QString("Com");
    if (genetive == "coronae austrinae")
        return QString("CrA");
    if (genetive == "coronae borealis")
        return QString("CrB");
    if (genetive == "corvi")
        return QString("Crv");
    if (genetive == "crateris")
        return QString("Crt");
    if (genetive == "crucis")
        return QString("Cru");
    if (genetive == "cygni")
        return QString("Cyg");
    if (genetive == "delphini")
        return QString("Del");
    if (genetive == "doradus")
        return QString("Dor");
    if (genetive == "draconis")
        return QString("Dra");
    if (genetive == "equulei")
        return QString("Equ");
    if (genetive == "eridani")
        return QString("Eri");
    if (genetive == "fornacis")
        return QString("For");
    if (genetive == "geminorum")
        return QString("Gem");
    if (genetive == "gruis")
        return QString("Gru");
    if (genetive == "herculis")
        return QString("Her");
    if (genetive == "horologii")
        return QString("Hor");
    if (genetive == "hydrae")
        return QString("Hya");
    if (genetive == "hydri")
        return QString("Hyi");
    if (genetive == "indi")
        return QString("Ind");
    if (genetive == "lacertae")
        return QString("Lac");
    if (genetive == "leonis")
        return QString("Leo");
    if (genetive == "leonis minoris")
        return QString("LMi");
    if (genetive == "leporis")
        return QString("Lep");
    if (genetive == "librae")
        return QString("Lib");
    if (genetive == "lupi")
        return QString("Lup");
    if (genetive == "lyncis")
        return QString("Lyn");
    if (genetive == "lyrae")
        return QString("Lyr");
    if (genetive == "mensae")
        return QString("Men");
    if (genetive == "microscopii")
        return QString("Mic");
    if (genetive == "monocerotis")
        return QString("Mon");
    if (genetive == "muscae")
        return QString("Mus");
    if (genetive == "normae")
        return QString("Nor");
    if (genetive == "octantis")
        return QString("Oct");
    if (genetive == "ophiuchi")
        return QString("Oph");
    if (genetive == "orionis")
        return QString("Ori");
    if (genetive == "pavonis")
        return QString("Pav");
    if (genetive == "pegasi")
        return QString("Peg");
    if (genetive == "persei")
        return QString("Per");
    if (genetive == "phoenicis")
        return QString("Phe");
    if (genetive == "pictoris")
        return QString("Pic");
    if (genetive == "piscium")
        return QString("Psc");
    if (genetive == "piscis austrini")
        return QString("PsA");
    if (genetive == "puppis")
        return QString("Pup");
    if (genetive == "pyxidis")
        return QString("Pyx");
    if (genetive == "reticuli")
        return QString("Ret");
    if (genetive == "sagittae")
        return QString("Sge");
    if (genetive == "sagittarii")
        return QString("Sgr");
    if (genetive == "scorpii")
        return QString("Sco");
    if (genetive == "sculptoris")
        return QString("Scl");
    if (genetive == "scuti")
        return QString("Sct");
    if (genetive == "serpentis")
        return QString("Ser");
    if (genetive == "sextantis")
        return QString("Sex");
    if (genetive == "tauri")
        return QString("Tau");
    if (genetive == "telescopii")
        return QString("Tel");
    if (genetive == "trianguli")
        return QString("Tri");
    if (genetive == "trianguli australis")
        return QString("TrA");
    if (genetive == "tucanae")
        return QString("Tuc");
    if (genetive == "ursae majoris")
        return QString("UMa");
    if (genetive == "ursae minoris")
        return QString("UMi");
    if (genetive == "velorum")
        return QString("Vel");
    if (genetive == "virginis")
        return QString("Vir");
    if (genetive == "volantis")
        return QString("Vol");
    if (genetive == "vulpeculae")
        return QString("Vul");
    return genetive_;
}

QString Logging::_filename;

void Logging::UseFile()
{
    if (_filename.isEmpty())
    {
        QDir dir;
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("logs/" +
                       QDateTime::currentDateTime().toString("yyyy-MM-dd"));
        dir.mkpath(path);
        QString name = "log_" + QDateTime::currentDateTime().toString("HH-mm-ss") + ".txt";
        _filename    = path + QStringLiteral("/") + name;

        // Clear file contents
        QFile file(_filename);
        file.open(QFile::WriteOnly);
        file.close();
    }

    qSetMessagePattern("[%{time yyyy-MM-dd h:mm:ss.zzz t} %{if-debug}DEBG%{endif}%{if-info}INFO%{endif}%{if-warning}WARN%{endif}%{if-critical}CRIT%{endif}%{if-fatal}FATL%{endif}] %{if-category}[%{category}]%{endif} - %{message}");
    qInstallMessageHandler(File);
}

void Logging::File(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QFile file(_filename);
    if (file.open(QFile::Append | QIODevice::Text))
    {
        QTextStream stream(&file);
        Write(stream, type, context, msg);
    }
}

void Logging::UseStdout()
{
    qSetMessagePattern("[%{time yyyy-MM-dd h:mm:ss.zzz t} %{if-debug}DEBG%{endif}%{if-info}INFO%{endif}%{if-warning}WARN%{endif}%{if-critical}CRIT%{endif}%{if-fatal}FATL%{endif}] %{if-category}[%{category}]%{endif} - %{message}");
    qInstallMessageHandler(Stdout);
}

void Logging::Stdout(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QTextStream stream(stdout, QIODevice::WriteOnly);
    Write(stream, type, context, msg);
}

void Logging::UseStderr()
{
    qInstallMessageHandler(Stderr);
}

void Logging::Stderr(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    Write(stream, type, context,  msg);
}

void Logging::Write(QTextStream &stream, QtMsgType type, const QMessageLogContext &context, const QString &msg)
{

    stream << QDateTime::currentDateTime().toString("[yyyy-MM-ddThh:mm:ss.zzz t ");

    switch (type)
    {
        case QtInfoMsg:
            stream << "INFO ]";
            break;
        case QtDebugMsg:
            stream << "DEBG ]";
            break;
        case QtWarningMsg:
            stream << "WARN ]";
            break;
        case QtCriticalMsg:
            stream << "CRIT ]";
            break;
        case QtFatalMsg:
            stream << "FATL ]";
            break;
        default:
            stream << "UNKN ]";
    }


    stream << "[" << qSetFieldWidth(30) << context.category << qSetFieldWidth(0) << "] - ";
    stream << msg << '\n';
    stream.flush();
    //stream << qFormatLogMessage(type, context, msg) << endl;
}

void Logging::UseDefault()
{
    qInstallMessageHandler(nullptr);
}

void Logging::Disable()
{
    qInstallMessageHandler(Disabled);
}

void Logging::Disabled(QtMsgType, const QMessageLogContext &, const QString &)
{
}

void Logging::SyncFilterRules()
{
    //    QString rules = QString("org.kde.kstars.ekos.debug=%1\n"
    //                            "org.kde.kstars.indi.debug=%2\n"
    //                            "org.kde.kstars.fits.debug=%3\n"
    //                            "org.kde.kstars.ekos.capture.debug=%4\n"
    //                            "org.kde.kstars.ekos.focus.debug=%5\n"
    //                            "org.kde.kstars.ekos.guide.debug=%6\n"
    //                            "org.kde.kstars.ekos.align.debug=%7\n"
    //                            "org.kde.kstars.ekos.mount.debug=%8\n"
    //                            "org.kde.kstars.ekos.scheduler.debug=%9\n").arg(
    //                        Options::verboseLogging() ? "true" : "false",
    //                        Options::iNDILogging() ? "true" : "false",
    //                        Options::fITSLogging() ? "true" : "false",
    //                        Options::captureLogging() ? "true" : "false",
    //                        Options::focusLogging() ? "true" : "false",
    //                        Options::guideLogging() ? "true" : "false",
    //                        Options::alignmentLogging() ? "true" : "false",
    //                        Options::mountLogging() ? "true" : "false",
    //                        Options::schedulerLogging() ? "true" : "false")
    //            .append(QString("org.kde.kstars.ekos.observatory.debug=%2\n"
    //                            "org.kde.kstars.debug=%1").arg(
    //                        Options::verboseLogging() ? "true" : "false",
    //                        Options::observatoryLogging() ? "true" : "false"));

    QStringList rules;

    rules << "org.kde.kstars.ekos.debug" << (Options::verboseLogging() ? "true" : "false");
    rules << "org.kde.kstars.indi.debug" << (Options::iNDILogging() ? "true" : "false");
    rules << "org.kde.kstars.fits.debug" << (Options::fITSLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.capture.debug" << (Options::captureLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.focus.debug" << (Options::focusLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.guide.debug" << (Options::guideLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.align.debug" << (Options::alignmentLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.mount.debug" << (Options::mountLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.scheduler.debug" << (Options::schedulerLogging() ? "true" : "false");
    rules << "org.kde.kstars.ekos.observatory.debug" << (Options::observatoryLogging() ? "true" : "false");
    rules << "org.kde.kstars.debug" << (Options::verboseLogging() ? "true" : "false");

    QString formattedRules;
    for (int i = 0; i < rules.size(); i += 2)
        formattedRules.append(QString("%1=%2\n").arg(rules[i], rules[i + 1]));

    QLoggingCategory::setFilterRules(formattedRules);
}

/**
  This method provides a centralized location for the default paths to important external files used in the Options
  on different operating systems.  Note that on OS X, if the user builds the app without indi, astrometry, and xplanet internally
  then the options below will be used.  If the user drags the app from a dmg and has to install the KStars data directory,
  then most of these paths will be overwritten since it is preferred to use the internal versions.
**/

QString getDefaultPath(const QString &option)
{
    // We support running within Snaps, Flatpaks, and AppImage
    // The path should accomodate the differences between the different
    // packaging solutions
    QString snap = QProcessEnvironment::systemEnvironment().value("SNAP");
    QString flat = QProcessEnvironment::systemEnvironment().value("FLATPAK_DEST");
    QString appimg = QProcessEnvironment::systemEnvironment().value("APPDIR");

    // User prefix is the primary mounting point
    QString userPrefix = "/usr";
    // By default /usr is the prefix
    QString prefix = userPrefix;
    // Detect if we are within an App Image
    if (QProcessEnvironment::systemEnvironment().value("APPIMAGE").isEmpty() == false &&
            appimg.isEmpty() == false)
        prefix = appimg + userPrefix;
    else if (flat.isEmpty() == false)
        // Detect if we are within a Flatpak
        prefix = flat;
    // Detect if we are within a snap
    else if (snap.isEmpty() == false)
        prefix = snap + userPrefix;


    if (option == "fitsDir")
    {
        return QDir::homePath();
    }
    else if (option == "indiServer")
    {
#if defined(INDI_PREFIX)
        return QString(INDI_PREFIX "/bin/indiserver");
#elif defined(Q_OS_OSX)
        return "/usr/local/bin/indiserver";
#endif
        return prefix + "/bin/indiserver";
    }
    else if (option == "INDIHubAgent")
    {
#if defined(INDI_PREFIX)
        return QString(INDI_PREFIX "/bin/indihub-agent");
#elif defined(Q_OS_OSX)
        return "/usr/local/bin/indihub-agent";
#endif
        return prefix + "/bin/indihub-agent";
    }
    else if (option == "indiDriversDir")
    {
#if defined(INDI_PREFIX)
        return QString(INDI_PREFIX "/share/indi");
#elif defined(Q_OS_OSX)
        return "/usr/local/share/indi";
#elif defined(Q_OS_LINUX)
        return prefix + "/share/indi";
#else
        return QStandardPaths::locate(QStandardPaths::AppDataLocation, "indi", QStandardPaths::LocateDirectory);
#endif
    }
    else if (option == "AstrometrySolverBinary")
    {
#if defined(ASTROMETRY_PREFIX)
        return QString(ASTROMETRY_PREFIX "/bin/solve-field");
#elif defined(Q_OS_OSX)
        return "/usr/local/bin/solve-field";
#elif defined(Q_OS_WIN)
        return QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/solve-field.exe";
#endif
        return prefix + "/bin/solve-field";
    }
    else if (option == "SextractorBinary")
    {
#if defined(SEXTRACTOR_PREFIX)
        return QString(SEXTRACTOR_PREFIX "/bin/sextractor");
#elif defined(Q_OS_OSX)
        return "/usr/local/bin/sex";
#endif
        return prefix + "/bin/sextractor";
    }
    else if (option == "AstrometryWCSInfo")
    {
#if defined(ASTROMETRY_PREFIX)
        return QString(ASTROMETRY_PREFIX "/bin/wcsinfo");
#elif defined(Q_OS_OSX)
        return "/usr/local/bin/wcsinfo";
#elif defined(Q_OS_WIN)
        return QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/wcsinfo.exe";
#endif
        return prefix + "/bin/wcsinfo";
    }
    else if (option == "AstrometryConfFile")
    {
#if defined(ASTROMETRY_CONF_IN_PREFIX) && defined(ASTROMETRY_PREFIX)
        return QString(ASTROMETRY_PREFIX "/etc/astrometry.cfg");
#elif defined(Q_OS_OSX)
        return "/usr/local/etc/astrometry.cfg";
#elif defined(Q_OS_WIN)
        return QDir::homePath() + "/AppData/Local/cygwin_ansvr/etc/astrometry/backend.cfg";
#endif
        // We move /usr
        prefix.remove(userPrefix);
        return prefix + "/etc/astrometry.cfg";
    }
    else if (option == "AstrometryIndexFileLocation")
    {
#if defined(ASTROMETRY_PREFIX)
        return QString(ASTROMETRY_PREFIX "/share/astrometry");
#elif defined(Q_OS_OSX)
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("Astrometry/");
#endif
        return prefix + "/share/astrometry/";
    }
    else if (option == "AstrometryLogFilepath")
    {
        return QDir::tempPath() + "/astrometryLog.txt";
    }
    else if (option == "XplanetPath")
    {
#if defined(XPLANET_PREFIX)
        return QString(XPLANET_PREFIX "/bin/xplanet");
#elif defined(Q_OS_OSX)
        return "/usr/local/bin/xplanet";
#endif
        return prefix + "/bin/xplanet";
    }
    else if (option == "ASTAP")
    {
#if defined(Q_OS_OSX)
        return "/Applications/ASTAP.app/Contents/MacOS/astap";
#elif defined(Q_OS_WIN)
        return "C:/Program Files/astap/astap.exe";
#endif
        return "/opt/astap/astap";
    }

    return QString();
}

QStringList getAstrometryDefaultIndexFolderPaths()
{
    QStringList folderPaths;
    const QString confDir = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QLatin1String("astrometry"));
    folderPaths << confDir;
    // Check if directory already exists, if it doesn't create one
    QDir writableDir(confDir);
    if (writableDir.exists() == false)
    {
        if (writableDir.mkdir(confDir) == false)
        {
            qCCritical(KSTARS) << "Failed to create local astrometry directory";
            folderPaths.clear();
        }
    }

#ifdef HAVE_STELLARSOLVER
    folderPaths.append(StellarSolver::getDefaultIndexFolderPaths());
#endif
    return folderPaths;
}

#if defined(Q_OS_OSX)
//Note that this will copy and will not overwrite, so that the user's changes in the files are preserved.
void copyResourcesFolderFromAppBundle(QString folder)
{
    QString folderLocation = QStandardPaths::locate(QStandardPaths::GenericDataLocation, folder,
                             QStandardPaths::LocateDirectory);
    QDir folderSourceDir;
    if(folder == "kstars")
        folderSourceDir = QDir(QCoreApplication::applicationDirPath() + "/../Resources/kstars").absolutePath();
    else
        folderSourceDir = QDir(QCoreApplication::applicationDirPath() + "/../Resources/" + folder).absolutePath();
    if (folderSourceDir.exists())
    {
        folderLocation = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + '/' + folder;
        QDir writableDir;
        writableDir.mkdir(folderLocation);
        copyRecursively(folderSourceDir.absolutePath(), folderLocation);
    }
}

bool setupMacKStarsIfNeeded() // This method will return false if the KStars data directory doesn't exist when it's done
{
    //This will copy the locale folder, the notifications folder, and the sounds folder and any missing files in them to Application Support if needed.
    copyResourcesFolderFromAppBundle("locale");
    copyResourcesFolderFromAppBundle("knotifications5");
    copyResourcesFolderFromAppBundle("sounds");

    //This will copy the KStars data directory
    copyResourcesFolderFromAppBundle("kstars");

    if(Options::kStarsFirstRun())
    {
        //This sets some important OS X options.
        Options::setIndiServerIsInternal(true);
        Options::setIndiServer("*Internal INDI Server*");
        Options::setIndiDriversAreInternal(true);
        Options::setIndiDriversDir("*Internal INDI Drivers*");
        Options::setAstrometrySolverIsInternal(true);
        Options::setAstrometrySolverBinary("*Internal Solver*");
        Options::setAstrometryConfFileIsInternal(true);
        Options::setAstrometryConfFile("*Internal astrometry.cfg*");
        Options::setSextractorIsInternal(true);
        Options::setSextractorBinary("*Internal Sextractor*");
        Options::setAstrometryWCSIsInternal(true);
        Options::setAstrometryWCSInfo("*Internal wcsinfo*");
        Options::setXplanetIsInternal(true);
        Options::setXplanetPath("*Internal XPlanet*");
    }

    QString dataLocation =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    if (dataLocation.isEmpty()) //If there is no kstars user data directory
        return false;

    return true;
}


//Can this and the linux method be merged somehow? See KSUtils::createLocalAstrometryConf
bool configureAstrometry()
{
    QStringList astrometryDataDirs = getAstrometryDataDirs();
    if (astrometryDataDirs.count() == 0)
        return false;
    QString defaultAstrometryDataDir = getDefaultPath("AstrometryIndexFileLocation");
    if(astrometryDataDirs.contains("IndexFileLocationNotYetSet"))
        replaceIndexFileNotYetSet();
    if (QDir(defaultAstrometryDataDir).exists() == false)
    {
        if (KMessageBox::warningYesNo(
                    nullptr, i18n("The selected Astrometry Index File Location:\n %1 \n does not exist.  Do you want to make the directory?",
                                  defaultAstrometryDataDir),
                    i18n("Make Astrometry Index File Directory?")) == KMessageBox::Yes)
        {
            if(QDir(defaultAstrometryDataDir).mkdir(defaultAstrometryDataDir))
            {
                KSNotification::info(i18n("The Default Astrometry Index File Location was created."));
            }
            else
            {
                KSNotification::sorry(i18n("The Default Astrometry Index File Directory does not exist and was not able to be created."));
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool replaceIndexFileNotYetSet()
{
    QString confPath = KSUtils::getAstrometryConfFilePath();

    QFile confFile(confPath);
    QString contents;
    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KSNotification::error( i18n("Astrometry Configuration File Read Error."));
        return false;
    }
    else
    {
        QByteArray fileContent = confFile.readAll();
        confFile.close();
        QString contents = QString::fromLatin1(fileContent);
        contents.replace("IndexFileLocationNotYetSet", getDefaultPath("AstrometryIndexFileLocation"));

        if (confFile.open(QIODevice::WriteOnly) == false)
        {
            KSNotification::error( i18n("Internal Astrometry Configuration File Write Error."));
            return false;
        }
        else
        {
            QTextStream out(&confFile);
            out << contents;
            confFile.close();
        }
    }
    return true;
}

bool copyRecursively(QString sourceFolder, QString destFolder)
{
    QDir sourceDir(sourceFolder);

    if (!sourceDir.exists())
        return false;

    QDir destDir(destFolder);
    if (!destDir.exists())
        destDir.mkdir(destFolder);

    QStringList files = sourceDir.entryList(QDir::Files);
    for (int i = 0; i < files.count(); i++)
    {
        QString srcName  = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
        QFile::copy(srcName, destName); //Note this does not overwrite files
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (int i = 0; i < files.count(); i++)
    {
        QString srcName  = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
        copyRecursively(srcName, destName);
    }

    return true;
}
#endif

#if 0
//Note maybe the Mac and Linux versions of creating the local astrometry conf file and index file folder can be merged.
//I moved both of them here and this method references each one separately.
//One is createLocalAstrometryConf and the other is configureAstrometry
bool configureLocalAstrometryConfIfNecessary()
{
#if defined(Q_OS_LINUX) || defined (Q_OS_WIN32)
    QString confPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("astrometry") +
                       QLatin1String("/astrometry.cfg");
    if (QFileInfo(confPath).exists() == false)
    {
        if(createLocalAstrometryConf() == false)
            return false;
    }
#elif defined(Q_OS_OSX)
    if(configureAstrometry() == false)
    {
        KMessageBox::information(
            nullptr,
            i18n(
                "Failed to properly configure astrometry config file.  Please click the options button in the lower right of the Astrometry Tab in Ekos to correct your settings.  Then try starting Ekos again."),
            i18n("Astrometry Config File Error"), "astrometry_configuration_failure_warning");
        return false;
    }
#endif
    return true;
}

//Can this and the mac method be merged somehow?  See KSUtils::configureAstrometry.
bool createLocalAstrometryConf()
{
    const QString confDir = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("astrometry");
    QString confPath = confDir + QDir::separator() + QLatin1String("astrometry.cfg");

    // Check if directory already exists, if it doesn't create one
    QDir writableDir(confPath);
    if (writableDir.exists() == false)
    {
        if (writableDir.mkdir(confPath) == false)
        {
            qCCritical(KSTARS) << "Failed to create local astrometry directory";
            return false;
        }
    }

#if defined(Q_OS_LINUX)
    // Now copy system astrometry.cfg to local directory
    const QString systemConfPath = "/etc/astrometry.cfg";
    if (QFile(systemConfPath).copy(confPath) == false)
    {
        qCCritical(KSTARS) << "Failed to copy" << systemConfPath << "to" << confPath;
        return false;
    }
    QFile localConf(confPath);
    // Open file and add our own path to it
    if (localConf.open(QFile::ReadWrite))
    {
        QString all = localConf.readAll();
        QStringList lines = all.split("\n");
        for (int i = 0; i < lines.count(); i++)
        {
            if (lines[i].startsWith("add_path"))
            {
                lines.insert(i + 1, QString("add_path %1").arg(confDir));
                break;
            }
        }

        // Clear contents
        localConf.resize(0);

        // Now write back all the lines including our own inserted above
        QTextStream out(&localConf);
        for(const QString &line : lines)
            out << line << '\n';
        localConf.close();
        return true;
    }
#elif defined(Q_OS_WIN32)
    QFile localConf(confPath);
    QTextStream out(&localConf);
    if (localConf.open(QFile::WriteOnly))
    {
        out << "inparallel" << endl;
        out << "cpulimit 300" << endl;
        out << QString("add_path %1astrometry").arg(confDir) << endl;
        out << "autoindex" << endl;
        localConf.close();
    }
#endif

    qCCritical(KSTARS) << "Failed to open local astrometry config" << confPath;
    return false;
}
#endif

QString getAstrometryConfFilePath()
{
#if defined(Q_OS_LINUX)
    if (Options::astrometryConfFileIsInternal())
        return QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QLatin1String("astrometry") +
               QLatin1String("/astrometry.cfg"));
#elif defined(Q_OS_OSX)
    if (Options::astrometryConfFileIsInternal())
        return QCoreApplication::applicationDirPath() + "/astrometry/bin/astrometry.cfg";
#endif

    return Options::astrometryConfFile();
}

QStringList getAstrometryDataDirs()
{
    QStringList optionsDataDirs = Options::astrometryIndexFolderList();

    bool updated = false;

    // Cleaning up the list of directories in options.
    for(int dir = 0; dir < optionsDataDirs.count(); dir++)
    {
        QString optionsDataDirName = optionsDataDirs.at(dir);
        QDir optionsDataDir(optionsDataDirName);
        if(optionsDataDir.exists())
        {
            //This will replace directory names that aren't the absolute path
            if(optionsDataDir.absolutePath() != optionsDataDirName)
            {
                optionsDataDirs.replace(dir, optionsDataDir.absolutePath());
                updated = true;
            }
        }
        else
        {
            //This removes directories that do not exist from the list.
            optionsDataDirs.removeAt(dir);
            dir--;
            updated = true;
        }
    }

    //This will load the conf file if it exists
    QFile confFile(KSUtils::getAstrometryConfFilePath());
    if (confFile.open(QIODevice::ReadOnly))
    {
        QStringList confDataDirs;
        QTextStream in(&confFile);
        QString line;

        //This will find the index file paths in the conf file
        while (!in.atEnd())
        {
            line = in.readLine();
            if (line.isEmpty() || line.startsWith('#'))
                continue;

            line = line.trimmed();
            if (line.startsWith(QLatin1String("add_path")))
            {
                confDataDirs << line.mid(9).trimmed();
            }
        }

        //This will search through the paths and compare them to the index folder list
        //It will add them if they aren't in there.
        for(QString astrometryDataDirName : confDataDirs)
        {
            QDir astrometryDataDir(astrometryDataDirName);
            //This rejects any that do not exist
            if(!astrometryDataDir.exists())
                continue;
            QString astrometryDataDirPath = astrometryDataDir.absolutePath();
            if( !optionsDataDirs.contains(astrometryDataDirPath ))
            {
                optionsDataDirs.append(astrometryDataDirPath);
                updated = true;
            }
        }
    }

    //This will remove any duplicate entries.
    if(optionsDataDirs.removeDuplicates() != 0)
        updated = true;

    //This updates the list in Options if it changed.
    if(updated)
        Options::setAstrometryIndexFolderList(optionsDataDirs);

    return optionsDataDirs;
}

bool addAstrometryDataDir(const QString &dataDir)
{
    //This will need to be fixed!
    //if(Options::astrometryIndexFileLocation() != dataDir)
    //    Options::setAstrometryIndexFileLocation(dataDir);

    QString confPath = KSUtils::getAstrometryConfFilePath();
    QStringList astrometryDataDirs = getAstrometryDataDirs();

    QFile confFile(confPath);
    QString contents;
    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KSNotification::error( i18n("Astrometry Configuration File Read Error."));
        return false;
    }
    else
    {
        QTextStream in(&confFile);
        QString line;
        bool foundSpot = false;
        while (!in.atEnd())
        {
            line = in.readLine();

            if (line.trimmed().startsWith(QLatin1String("add_path")))
            {
                if (!foundSpot)
                {
                    foundSpot = true;
                    for(QString astrometryDataDir : astrometryDataDirs)
                        contents += "add_path " + astrometryDataDir + '\n';
                    contents += "add_path " + dataDir + '\n';
                }
                else
                {
                    //Do not keep adding the other add_paths because they just got added in the seciton above.
                }
            }
            else
            {
                contents += line + '\n';
            }
        }
        if(!foundSpot)
        {
            for(QString astrometryDataDir : astrometryDataDirs)
                contents += "add_path " + astrometryDataDir + '\n';
            contents += "add_path " + dataDir + '\n';
        }

        confFile.close();

        if (confFile.open(QIODevice::WriteOnly) == false)
        {
            KSNotification::error( i18n("Internal Astrometry Configuration File Write Error."));
            return false;
        }
        else
        {
            QTextStream out(&confFile);
            out << contents;
            confFile.close();
        }
    }
    return true;
}

bool removeAstrometryDataDir(const QString &dataDir)
{
    QString confPath = KSUtils::getAstrometryConfFilePath();
    QStringList astrometryDataDirs = getAstrometryDataDirs();

    QFile confFile(confPath);
    QString contents;
    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KSNotification::error( i18n("Astrometry Configuration File Read Error."));
        return false;
    }
    else
    {
        QTextStream in(&confFile);
        QString line;
        while (!in.atEnd())
        {
            line = in.readLine();
            if (line.mid(9).trimmed() != dataDir)
            {
                contents += line + '\n';
            }

        }
        confFile.close();

        if (confFile.open(QIODevice::WriteOnly) == false)
        {
            KSNotification::error( i18n("Internal Astrometry Configuration File Write Error."));
            return false;
        }
        else
        {
            QTextStream out(&confFile);
            out << contents;
            confFile.close();
        }
    }
    return true;
}
QByteArray getJPLQueryString(const QByteArray &kind, const QByteArray &dataFields, const QVector<JPLFilter> &filters)
{
    QByteArray query("obj_group=all&obj_kind=" + kind + "&obj_numbered=all&OBJ_field=0&ORB_field=0");

    // Apply filters:
    for (int i = 0; i < filters.length(); i++)
    {
        QByteArray f = QByteArray::number(i + 1);
        query += "&c" + f + "_group=OBJ&c1_item=" + filters[i].item + "&c" + f + "_op=" + filters[i].op + "&c" + f +
                 "_value=" + filters[i].value;
    }

    // Apply query data fields...
    query += "&c_fields=" + dataFields;

    query += "&table_format=CSV&max_rows=10&format_option=full&query=Generate%20Table&."
             "cgifields=format_option&.cgifields=field_list&.cgifields=obj_kind&.cgifie"
             "lds=obj_group&.cgifields=obj_numbered&.cgifields=combine_mode&.cgifields="
             "ast_orbit_class&.cgifields=table_format&.cgifields=ORB_field_set&.cgifiel"
             "ds=OBJ_field_set&.cgifields=preset_field_set&.cgifields=com_orbit_class";

    return query;
}

bool RAWToJPEG(const QString &rawImage, const QString &output, QString &errorMessage)
{
#ifndef HAVE_LIBRAW
    errorMessage = i18n("Unable to find dcraw and cjpeg. Please install the required tools to convert CR2/NEF to JPEG.");
    return false;
#else
    int ret = 0;
    // Creation of image processing object
    LibRaw RawProcessor;

    // Let us open the file
    if ((ret = RawProcessor.open_file(rawImage.toLatin1().data())) != LIBRAW_SUCCESS)
    {
        errorMessage = i18n("Cannot open %1: %2", rawImage, libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    // Let us unpack the thumbnail
    if ((ret = RawProcessor.unpack_thumb()) != LIBRAW_SUCCESS)
    {
        errorMessage = i18n("Cannot unpack_thumb %1: %2", rawImage, libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }
    else
        // We have successfully unpacked the thumbnail, now let us write it to a file
    {
        //snprintf(thumbfn,sizeof(thumbfn),"%s.%s",av[i],T.tformat == LIBRAW_THUMBNAIL_JPEG ? "thumb.jpg" : "thumb.ppm");
        if (LIBRAW_SUCCESS != (ret = RawProcessor.dcraw_thumb_writer(output.toLatin1().data())))
        {
            errorMessage = i18n("Cannot write %s %1: %2", output, libraw_strerror(ret));
            RawProcessor.recycle();
            return false;
        }
    }
    return true;
#endif
}

double getAvailableRAM()
{
#if defined(Q_OS_OSX)
    int mib [] = { CTL_HW, HW_MEMSIZE };
    size_t length;
    length = sizeof(int64_t);
    int64_t RAMcheck;
    if(sysctl(mib, 2, &RAMcheck, &length, NULL, 0))
        return false; // On Error
    //Until I can figure out how to get free RAM on Mac
    return RAMcheck;
#elif defined(Q_OS_LINUX)
    QProcess p;
    p.start("awk", QStringList() << "/MemAvailable/ { print $2 }" << "/proc/meminfo");
    p.waitForFinished();
    QString memory = p.readAllStandardOutput();
    p.close();
    //kB to bytes
    return (memory.toLong() * 1024.0);
#elif defined(Q_OS_WIN32)
    MEMORYSTATUSEX memory_status;
    ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
    memory_status.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memory_status))
    {
        return memory_status.ullAvailPhys;
    }
    else
    {
        return 0;
    }
#endif
    return 0;
}

}
