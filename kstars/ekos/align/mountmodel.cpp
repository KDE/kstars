/*  Ekos Mount MOdel
    SPDX-FileCopyrightText: 2018 Robert Lancaster

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mountmodel.h"

#include "align.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "flagcomponent.h"
#include "ksnotification.h"

#include "skymap.h"
#include "starobject.h"
#include "skymapcomposite.h"
#include "skyobject.h"
#include "dialogs/finddialog.h"
#include "QProgressIndicator.h"

#include <ekos_align_debug.h>

#define AL_FORMAT_VERSION 1.0

namespace Ekos
{
MountModel::MountModel(Align *parent) : QDialog(parent)
{
    setupUi(this);

    m_AlignInstance = parent;

    setWindowTitle("Mount Model Tool");
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    alignTable->setColumnWidth(0, 70);
    alignTable->setColumnWidth(1, 75);
    alignTable->setColumnWidth(2, 130);
    alignTable->setColumnWidth(3, 30);

    wizardAlignB->setIcon(
        QIcon::fromTheme("tools-wizard"));
    wizardAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    clearAllAlignB->setIcon(
        QIcon::fromTheme("application-exit"));
    clearAllAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    removeAlignB->setIcon(QIcon::fromTheme("list-remove"));
    removeAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    addAlignB->setIcon(QIcon::fromTheme("list-add"));
    addAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    findAlignB->setIcon(QIcon::fromTheme("edit-find"));
    findAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    alignTable->verticalHeader()->setDragDropOverwriteMode(false);
    alignTable->verticalHeader()->setSectionsMovable(true);
    alignTable->verticalHeader()->setDragEnabled(true);
    alignTable->verticalHeader()->setDragDropMode(QAbstractItemView::InternalMove);
    connect(alignTable->verticalHeader(), SIGNAL(sectionMoved(int, int, int)), this,
            SLOT(moveAlignPoint(int, int, int)));

    loadAlignB->setIcon(
        QIcon::fromTheme("document-open"));
    loadAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    saveAsAlignB->setIcon(
        QIcon::fromTheme("document-save-as"));
    saveAsAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    saveAlignB->setIcon(
        QIcon::fromTheme("document-save"));
    saveAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    previewB->setIcon(QIcon::fromTheme("kstars_grid"));
    previewB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    previewB->setCheckable(true);

    sortAlignB->setIcon(QIcon::fromTheme("svn-update"));
    sortAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    stopAlignB->setIcon(
        QIcon::fromTheme("media-playback-stop"));
    stopAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    startAlignB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    startAlignB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(wizardAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotWizardAlignmentPoints);
    connect(alignTypeBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::MountModel::alignTypeChanged);

    connect(starListBox, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), this,
            &Ekos::MountModel::slotStarSelected);
    connect(greekStarListBox, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            this,
            &Ekos::MountModel::slotStarSelected);

    connect(loadAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotLoadAlignmentPoints);
    connect(saveAsAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotSaveAsAlignmentPoints);
    connect(saveAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotSaveAlignmentPoints);
    connect(clearAllAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotClearAllAlignPoints);
    connect(removeAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotRemoveAlignPoint);
    connect(addAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotAddAlignPoint);
    connect(findAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotFindAlignObject);
    connect(sortAlignB, &QPushButton::clicked, this, &Ekos::MountModel::slotSortAlignmentPoints);

    connect(previewB, &QPushButton::clicked, this, &Ekos::MountModel::togglePreviewAlignPoints);
    connect(stopAlignB, &QPushButton::clicked, this, &Ekos::MountModel::resetAlignmentProcedure);
    connect(startAlignB, &QPushButton::clicked, this, &Ekos::MountModel::startStopAlignmentProcedure);

    generateAlignStarList();

}

MountModel::~MountModel()
{

}

void MountModel::generateAlignStarList()
{
    alignStars.clear();
    starListBox->clear();
    greekStarListBox->clear();

    KStarsData *data = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> listStars;
    listStars.append(data->skyComposite()->objectLists(SkyObject::STAR));
    for (int i = 0; i < listStars.size(); i++)
    {
        QPair<QString, const SkyObject *> pair = listStars.value(i);
        const StarObject *star                 = dynamic_cast<const StarObject *>(pair.second);
        if (star)
        {
            StarObject *alignStar = star->clone();
            alignStar->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
            alignStars.append(alignStar);
        }
    }

    QStringList boxNames;
    QStringList greekBoxNames;

    for (int i = 0; i < alignStars.size(); i++)
    {
        const StarObject *star = alignStars.value(i);
        if (star)
        {
            if (!isVisible(star))
            {
                alignStars.remove(i);
                i--;
            }
            else
            {
                if (star->hasLatinName())
                    boxNames << star->name();
                else
                {
                    if (!star->gname().isEmpty())
                        greekBoxNames << star->gname().simplified();
                }
            }
        }
    }

    boxNames.sort(Qt::CaseInsensitive);
    boxNames.removeDuplicates();
    greekBoxNames.removeDuplicates();
    std::sort(greekBoxNames.begin(), greekBoxNames.end(), [](const QString & a, const QString & b)
    {
        QStringList aParts = a.split(' ');
        QStringList bParts = b.split(' ');
        if (aParts.length() < 2 || bParts.length() < 2)
            return a < b; //This should not happen, they should all have 2 words in the string.
        if (aParts[1] == bParts[1])
        {
            return aParts[0] < bParts[0]; //This compares the greek letter when the constellation is the same
        }
        else
            return aParts[1] < bParts[1]; //This compares the constellation names
    });

    starListBox->addItem("Select one:");
    greekStarListBox->addItem("Select one:");
    for (int i = 0; i < boxNames.size(); i++)
        starListBox->addItem(boxNames.at(i));
    for (int i = 0; i < greekBoxNames.size(); i++)
        greekStarListBox->addItem(greekBoxNames.at(i));
}

bool MountModel::isVisible(const SkyObject *so)
{
    return (getAltitude(so) > 30);
}

double MountModel::getAltitude(const SkyObject *so)
{
    KStarsData *data  = KStarsData::Instance();
    SkyPoint sp       = so->recomputeCoords(data->ut(), data->geo());

    //check altitude of object at this time.
    sp.EquatorialToHorizontal(data->lst(), data->geo()->lat());

    return sp.alt().Degrees();
}

void MountModel::togglePreviewAlignPoints()
{
    previewShowing = !previewShowing;
    previewB->setChecked(previewShowing);
    updatePreviewAlignPoints();
}

void MountModel::updatePreviewAlignPoints()
{
    FlagComponent *flags = KStarsData::Instance()->skyComposite()->flags();
    for (int i = 0; i < flags->size(); i++)
    {
        if (flags->label(i).startsWith(QLatin1String("Align")))
        {
            flags->remove(i);
            i--;
        }
    }
    if (previewShowing)
    {
        for (int i = 0; i < alignTable->rowCount(); i++)
        {
            QTableWidgetItem *raCell      = alignTable->item(i, 0);
            QTableWidgetItem *deCell      = alignTable->item(i, 1);
            QTableWidgetItem *objNameCell = alignTable->item(i, 2);

            if (raCell && deCell && objNameCell)
            {
                QString raString = raCell->text();
                QString deString = deCell->text();
                dms raDMS        = dms::fromString(raString, false);
                dms decDMS       = dms::fromString(deString, true);

                QString objString = objNameCell->text();

                SkyPoint flagPoint(raDMS, decDMS);
                flags->add(flagPoint, "J2000", "Default", "Align " + QString::number(i + 1) + ' ' + objString, "white");
            }
        }
    }
    KStars::Instance()->map()->forceUpdate(true);
}

void MountModel::slotLoadAlignmentPoints()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(this, i18nc("@title:window", "Open Ekos Alignment List"),
                   alignURLPath,
                   "Ekos AlignmentList (*.eal)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    alignURLPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    loadAlignmentPoints(fileURL.toLocalFile());
    if (previewShowing)
        updatePreviewAlignPoints();
}

bool MountModel::loadAlignmentPoints(const QString &fileURL)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    alignTable->setRowCount(0);

    LilXML *xmlParser = newLilXML();

    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            double sqVersion = atof(findXMLAttValu(root, "version"));
            if (sqVersion < AL_FORMAT_VERSION)
            {
                emit newLog(i18n("Deprecated sequence file format version %1. Please construct a new sequence file.",
                                 sqVersion));
                return false;
            }

            XMLEle *ep    = nullptr;
            XMLEle *subEP = nullptr;

            int currentRow = 0;

            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "AlignmentPoint"))
                {
                    alignTable->insertRow(currentRow);

                    subEP = findXMLEle(ep, "RA");
                    if (subEP)
                    {
                        QTableWidgetItem *RAReport = new QTableWidgetItem();
                        RAReport->setText(pcdataXMLEle(subEP));
                        RAReport->setTextAlignment(Qt::AlignHCenter);
                        alignTable->setItem(currentRow, 0, RAReport);
                    }
                    else
                        return false;
                    subEP = findXMLEle(ep, "DE");
                    if (subEP)
                    {
                        QTableWidgetItem *DEReport = new QTableWidgetItem();
                        DEReport->setText(pcdataXMLEle(subEP));
                        DEReport->setTextAlignment(Qt::AlignHCenter);
                        alignTable->setItem(currentRow, 1, DEReport);
                    }
                    else
                        return false;
                    subEP = findXMLEle(ep, "NAME");
                    if (subEP)
                    {
                        QTableWidgetItem *ObjReport = new QTableWidgetItem();
                        ObjReport->setText(pcdataXMLEle(subEP));
                        ObjReport->setTextAlignment(Qt::AlignHCenter);
                        alignTable->setItem(currentRow, 2, ObjReport);
                    }
                    else
                        return false;
                }
                currentRow++;
            }
            return true;
        }
    }
    return false;
}

void MountModel::slotSaveAsAlignmentPoints()
{
    alignURL.clear();
    slotSaveAlignmentPoints();
}

void MountModel::slotSaveAlignmentPoints()
{
    QUrl backupCurrent = alignURL;

    if (alignURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || alignURL.toLocalFile().contains("/Temp"))
        alignURL.clear();

    if (alignURL.isEmpty())
    {
        alignURL = QFileDialog::getSaveFileUrl(this, i18nc("@title:window", "Save Ekos Alignment List"), alignURLPath,
                                               "Ekos Alignment List (*.eal)");
        // if user presses cancel
        if (alignURL.isEmpty())
        {
            alignURL = backupCurrent;
            return;
        }

        alignURLPath = QUrl(alignURL.url(QUrl::RemoveFilename));

        if (alignURL.toLocalFile().endsWith(QLatin1String(".eal")) == false)
            alignURL.setPath(alignURL.toLocalFile() + ".eal");

        if (QFile::exists(alignURL.toLocalFile()))
        {
            int r = KMessageBox::warningContinueCancel(nullptr,
                    i18n("A file named \"%1\" already exists. "
                         "Overwrite it?",
                         alignURL.fileName()),
                    i18n("Overwrite File?"), KStandardGuiItem::overwrite());
            if (r == KMessageBox::Cancel)
                return;
        }
    }

    if (alignURL.isValid())
    {
        if ((saveAlignmentPoints(alignURL.toLocalFile())) == false)
        {
            KSNotification::error(i18n("Failed to save alignment list"), i18n("Save"));
            return;
        }
    }
    else
    {
        QString message = i18n("Invalid URL: %1", alignURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
    }
}

bool MountModel::saveAlignmentPoints(const QString &path)
{
    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<AlignmentList version='" << AL_FORMAT_VERSION << "'>" << endl;

    for (int i = 0; i < alignTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell      = alignTable->item(i, 0);
        QTableWidgetItem *deCell      = alignTable->item(i, 1);
        QTableWidgetItem *objNameCell = alignTable->item(i, 2);

        if (!raCell || !deCell || !objNameCell)
            return false;
        QString raString  = raCell->text();
        QString deString  = deCell->text();
        QString objString = objNameCell->text();

        outstream << "<AlignmentPoint>" << endl;
        outstream << "<RA>" << raString << "</RA>" << endl;
        outstream << "<DE>" << deString << "</DE>" << endl;
        outstream << "<NAME>" << objString << "</NAME>" << endl;
        outstream << "</AlignmentPoint>" << endl;
    }
    outstream << "</AlignmentList>" << endl;
    emit newLog(i18n("Alignment List saved to %1", path));
    file.close();
    return true;
}

void MountModel::slotSortAlignmentPoints()
{
    int firstAlignmentPt = findClosestAlignmentPointToTelescope();
    if (firstAlignmentPt != -1)
    {
        swapAlignPoints(firstAlignmentPt, 0);
    }

    for (int i = 0; i < alignTable->rowCount() - 1; i++)
    {
        int nextAlignmentPoint = findNextAlignmentPointAfter(i);
        if (nextAlignmentPoint != -1)
        {
            swapAlignPoints(nextAlignmentPoint, i + 1);
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

int MountModel::findClosestAlignmentPointToTelescope()
{
    dms bestDiff = dms(360);
    double index = -1;

    for (int i = 0; i < alignTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell = alignTable->item(i, 0);
        QTableWidgetItem *deCell = alignTable->item(i, 1);

        if (raCell && deCell)
        {
            dms raDMS = dms::fromString(raCell->text(), false);
            dms deDMS = dms::fromString(deCell->text(), true);

            SkyPoint sk(raDMS, deDMS);
            dms thisDiff = telescopeCoord.angularDistanceTo(&sk);
            if (thisDiff.Degrees() < bestDiff.Degrees())
            {
                index    = i;
                bestDiff = thisDiff;
            }
        }
    }
    return index;
}

int MountModel::findNextAlignmentPointAfter(int currentSpot)
{
    QTableWidgetItem *currentRACell = alignTable->item(currentSpot, 0);
    QTableWidgetItem *currentDECell = alignTable->item(currentSpot, 1);

    if (currentRACell && currentDECell)
    {
        dms thisRADMS = dms::fromString(currentRACell->text(), false);
        dms thisDEDMS = dms::fromString(currentDECell->text(), true);

        SkyPoint thisPt(thisRADMS, thisDEDMS);

        dms bestDiff = dms(360);
        double index = -1;

        for (int i = currentSpot + 1; i < alignTable->rowCount(); i++)
        {
            QTableWidgetItem *raCell = alignTable->item(i, 0);
            QTableWidgetItem *deCell = alignTable->item(i, 1);

            if (raCell && deCell)
            {
                dms raDMS = dms::fromString(raCell->text(), false);
                dms deDMS = dms::fromString(deCell->text(), true);
                SkyPoint point(raDMS, deDMS);
                dms thisDiff = thisPt.angularDistanceTo(&point);

                if (thisDiff.Degrees() < bestDiff.Degrees())
                {
                    index    = i;
                    bestDiff = thisDiff;
                }
            }
        }
        return index;
    }
    else
        return -1;
}

void MountModel::slotWizardAlignmentPoints()
{
    int points = alignPtNum->value();
    if (points <
            2)      //The minimum is 2 because the wizard calculations require the calculation of an angle between points.
        return; //It should not be less than 2 because the minimum in the spin box is 2.

    int minAlt       = minAltBox->value();
    KStarsData *data = KStarsData::Instance();
    GeoLocation *geo = data->geo();
    double lat       = geo->lat()->Degrees();

    if (alignTypeBox->currentIndex() == OBJECT_FIXED_DEC)
    {
        double decAngle = alignDec->value();
        //Dec that never rises.
        if (lat > 0)
        {
            if (decAngle < lat - 90 + minAlt) //Min altitude possible at minAlt deg above horizon
            {
                KSNotification::sorry(i18n("DEC is below the altitude limit"));
                return;
            }
        }
        else
        {
            if (decAngle > lat + 90 - minAlt) //Max altitude possible at minAlt deg above horizon
            {
                KSNotification::sorry(i18n("DEC is below the altitude limit"));
                return;
            }
        }
    }

    //If there are less than 6 points, keep them all in the same DEC,
    //any more, set the num per row to be the sqrt of the points to evenly distribute in RA and DEC
    int numRAperDEC = 5;
    if (points > 5)
        numRAperDEC = qSqrt(points);

    //These calculations rely on modulus and int division counting beginning at 0, but the #s start at 1.
    int decPoints       = (points - 1) / numRAperDEC + 1;
    int lastSetRAPoints = (points - 1) % numRAperDEC + 1;

    double decIncrement = -1;
    double initDEC      = -1;
    SkyPoint spTest;

    if (alignTypeBox->currentIndex() == OBJECT_FIXED_DEC)
    {
        decPoints    = 1;
        initDEC      = alignDec->value();
        decIncrement = 0;
    }
    else if (decPoints == 1)
    {
        decIncrement = 0;
        spTest.setAlt(
            minAlt); //The goal here is to get the point exactly West at the minAlt so that we can use that DEC
        spTest.setAz(270);
        spTest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        initDEC = spTest.dec().Degrees();
    }
    else
    {
        spTest.setAlt(
            minAlt +
            10); //We don't want to be right at the minAlt because there would be only 1 point on the dec circle above the alt.
        spTest.setAz(180);
        spTest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        initDEC = spTest.dec().Degrees();
        if (lat > 0)
            decIncrement = (80 - initDEC) / (decPoints); //Don't quite want to reach NCP
        else
            decIncrement = (initDEC - 80) / (decPoints); //Don't quite want to reach SCP
    }

    for (int d = 0; d < decPoints; d++)
    {
        double initRA      = -1;
        double raPoints    = -1;
        double raIncrement = -1;
        double dec;

        if (lat > 0)
            dec = initDEC + d * decIncrement;
        else
            dec = initDEC - d * decIncrement;

        if (alignTypeBox->currentIndex() == OBJECT_FIXED_DEC)
        {
            raPoints = points;
        }
        else if (d == decPoints - 1)
        {
            raPoints = lastSetRAPoints;
        }
        else
        {
            raPoints = numRAperDEC;
        }

        //This computes both the initRA and the raIncrement.
        calculateAngleForRALine(raIncrement, initRA, dec, lat, raPoints, minAlt);

        if (raIncrement == -1 || decIncrement == -1)
        {
            KSNotification::sorry(i18n("Point calculation error."));
            return;
        }

        for (int i = 0; i < raPoints; i++)
        {
            double ra = initRA + i * raIncrement;

            const SkyObject *original = getWizardAlignObject(ra, dec);

            QString ra_report, dec_report, name;

            if (original)
            {
                SkyObject *o = original->clone();
                o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
                getFormattedCoords(o->ra0().Hours(), o->dec0().Degrees(), ra_report, dec_report);
                name = o->longname();
            }
            else
            {
                getFormattedCoords(dms(ra).Hours(), dec, ra_report, dec_report);
                name = i18n("Sky Point");
            }

            int currentRow = alignTable->rowCount();
            alignTable->insertRow(currentRow);

            QTableWidgetItem *RAReport = new QTableWidgetItem();
            RAReport->setText(ra_report);
            RAReport->setTextAlignment(Qt::AlignHCenter);
            alignTable->setItem(currentRow, 0, RAReport);

            QTableWidgetItem *DECReport = new QTableWidgetItem();
            DECReport->setText(dec_report);
            DECReport->setTextAlignment(Qt::AlignHCenter);
            alignTable->setItem(currentRow, 1, DECReport);

            QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
            ObjNameReport->setText(name);
            ObjNameReport->setTextAlignment(Qt::AlignHCenter);
            alignTable->setItem(currentRow, 2, ObjNameReport);

            QTableWidgetItem *disabledBox = new QTableWidgetItem();
            disabledBox->setFlags(Qt::ItemIsSelectable);
            alignTable->setItem(currentRow, 3, disabledBox);
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

void MountModel::calculateAngleForRALine(double &raIncrement, double &initRA, double initDEC, double lat, double raPoints,
        double minAlt)
{
    SkyPoint spEast;
    SkyPoint spWest;

    //Circumpolar dec
    if (fabs(initDEC) > (90 - fabs(lat) + minAlt))
    {
        if (raPoints > 1)
            raIncrement = 360 / (raPoints - 1);
        else
            raIncrement = 0;
        initRA = 0;
    }
    else
    {
        dms AZEast, AZWest;
        calculateAZPointsForDEC(dms(initDEC), dms(minAlt), AZEast, AZWest);

        spEast.setAlt(minAlt);
        spEast.setAz(AZEast.Degrees());
        spEast.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

        spWest.setAlt(minAlt);
        spWest.setAz(AZWest.Degrees());
        spWest.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

        dms angleSep = spEast.ra().deltaAngle(spWest.ra());

        initRA = spWest.ra().Degrees();
        if (raPoints > 1)
            raIncrement = fabs(angleSep.Degrees() / (raPoints - 1));
        else
            raIncrement = 0;
    }
}

void MountModel::calculateAZPointsForDEC(dms dec, dms alt, dms &AZEast, dms &AZWest)
{
    KStarsData *data = KStarsData::Instance();
    GeoLocation *geo = data->geo();
    double AZRad;

    double sindec, cosdec, sinlat, coslat;
    double sinAlt, cosAlt;

    geo->lat()->SinCos(sinlat, coslat);
    dec.SinCos(sindec, cosdec);
    alt.SinCos(sinAlt, cosAlt);

    double arg = (sindec - sinlat * sinAlt) / (coslat * cosAlt);
    AZRad      = acos(arg);
    AZEast.setRadians(AZRad);
    AZWest.setRadians(2.0 * dms::PI - AZRad);
}

const SkyObject *MountModel::getWizardAlignObject(double ra, double dec)
{
    double maxSearch = 5.0;
    switch (alignTypeBox->currentIndex())
    {
        case OBJECT_ANY_OBJECT:
            return KStarsData::Instance()->skyComposite()->objectNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
        case OBJECT_FIXED_DEC:
        case OBJECT_FIXED_GRID:
            return nullptr;

        case OBJECT_ANY_STAR:
            return KStarsData::Instance()->skyComposite()->starNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
    }

    //If they want named stars, then try to search for and return the closest Align Star to the requested location

    dms bestDiff = dms(360);
    double index = -1;
    for (int i = 0; i < alignStars.size(); i++)
    {
        const StarObject *star = alignStars.value(i);
        if (star)
        {
            if (star->hasName())
            {
                SkyPoint thisPt(ra / 15.0, dec);
                dms thisDiff = thisPt.angularDistanceTo(star);
                if (thisDiff.Degrees() < bestDiff.Degrees())
                {
                    index    = i;
                    bestDiff = thisDiff;
                }
            }
        }
    }
    if (index == -1)
        return KStarsData::Instance()->skyComposite()->starNearest(new SkyPoint(dms(ra), dms(dec)), maxSearch);
    return alignStars.value(index);
}

void MountModel::alignTypeChanged(int alignType)
{
    if (alignType == OBJECT_FIXED_DEC)
        alignDec->setEnabled(true);
    else
        alignDec->setEnabled(false);
}

void MountModel::slotStarSelected(const QString selectedStar)
{
    for (int i = 0; i < alignStars.size(); i++)
    {
        const StarObject *star = alignStars.value(i);
        if (star)
        {
            if (star->name() == selectedStar || star->gname().simplified() == selectedStar)
            {
                int currentRow = alignTable->rowCount();
                alignTable->insertRow(currentRow);

                QString ra_report, dec_report;
                getFormattedCoords(star->ra0().Hours(), star->dec0().Degrees(), ra_report, dec_report);

                QTableWidgetItem *RAReport = new QTableWidgetItem();
                RAReport->setText(ra_report);
                RAReport->setTextAlignment(Qt::AlignHCenter);
                alignTable->setItem(currentRow, 0, RAReport);

                QTableWidgetItem *DECReport = new QTableWidgetItem();
                DECReport->setText(dec_report);
                DECReport->setTextAlignment(Qt::AlignHCenter);
                alignTable->setItem(currentRow, 1, DECReport);

                QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
                ObjNameReport->setText(star->longname());
                ObjNameReport->setTextAlignment(Qt::AlignHCenter);
                alignTable->setItem(currentRow, 2, ObjNameReport);

                QTableWidgetItem *disabledBox = new QTableWidgetItem();
                disabledBox->setFlags(Qt::ItemIsSelectable);
                alignTable->setItem(currentRow, 3, disabledBox);

                starListBox->setCurrentIndex(0);
                greekStarListBox->setCurrentIndex(0);
                return;
            }
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}


void MountModel::getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str)
{
    dms ra_s, dec_s;
    ra_s.setH(ra);
    dec_s.setD(dec);

    ra_str = QString("%1:%2:%3")
             .arg(ra_s.hour(), 2, 10, QChar('0'))
             .arg(ra_s.minute(), 2, 10, QChar('0'))
             .arg(ra_s.second(), 2, 10, QChar('0'));
    if (dec_s.Degrees() < 0)
        dec_str = QString("-%1:%2:%3")
                  .arg(abs(dec_s.degree()), 2, 10, QChar('0'))
                  .arg(abs(dec_s.arcmin()), 2, 10, QChar('0'))
                  .arg(dec_s.arcsec(), 2, 10, QChar('0'));
    else
        dec_str = QString("%1:%2:%3")
                  .arg(dec_s.degree(), 2, 10, QChar('0'))
                  .arg(dec_s.arcmin(), 2, 10, QChar('0'))
                  .arg(dec_s.arcsec(), 2, 10, QChar('0'));
}

void MountModel::slotClearAllAlignPoints()
{
    if (alignTable->rowCount() == 0)
        return;

    if (KMessageBox::questionYesNo(this, i18n("Are you sure you want to clear all the alignment points?"),
                                   i18n("Clear Align Points")) == KMessageBox::Yes)
        alignTable->setRowCount(0);

    if (previewShowing)
        updatePreviewAlignPoints();
}

void MountModel::slotRemoveAlignPoint()
{
    alignTable->removeRow(alignTable->currentRow());
    if (previewShowing)
        updatePreviewAlignPoints();
}

void MountModel::moveAlignPoint(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    Q_UNUSED(logicalIndex)

    for (int i = 0; i < alignTable->columnCount(); i++)
    {
        QTableWidgetItem *oldItem = alignTable->takeItem(oldVisualIndex, i);
        QTableWidgetItem *newItem = alignTable->takeItem(newVisualIndex, i);

        alignTable->setItem(newVisualIndex, i, oldItem);
        alignTable->setItem(oldVisualIndex, i, newItem);
    }
    alignTable->verticalHeader()->blockSignals(true);
    alignTable->verticalHeader()->moveSection(newVisualIndex, oldVisualIndex);
    alignTable->verticalHeader()->blockSignals(false);

    if (previewShowing)
        updatePreviewAlignPoints();
}

void MountModel::swapAlignPoints(int firstPt, int secondPt)
{
    for (int i = 0; i < alignTable->columnCount(); i++)
    {
        QTableWidgetItem *firstPtItem  = alignTable->takeItem(firstPt, i);
        QTableWidgetItem *secondPtItem = alignTable->takeItem(secondPt, i);

        alignTable->setItem(firstPt, i, secondPtItem);
        alignTable->setItem(secondPt, i, firstPtItem);
    }
}

void MountModel::slotAddAlignPoint()
{
    int currentRow = alignTable->rowCount();
    alignTable->insertRow(currentRow);

    QTableWidgetItem *disabledBox = new QTableWidgetItem();
    disabledBox->setFlags(Qt::ItemIsSelectable);
    alignTable->setItem(currentRow, 3, disabledBox);
}

void MountModel::slotFindAlignObject()
{
    if (FindDialog::Instance()->execWithParent(this) == QDialog::Accepted)
    {
        SkyObject *object = FindDialog::Instance()->targetObject();
        if (object != nullptr)
        {
            KStarsData * const data = KStarsData::Instance();

            SkyObject *o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
            int currentRow = alignTable->rowCount();
            alignTable->insertRow(currentRow);

            QString ra_report, dec_report;
            getFormattedCoords(o->ra0().Hours(), o->dec0().Degrees(), ra_report, dec_report);

            QTableWidgetItem *RAReport = new QTableWidgetItem();
            RAReport->setText(ra_report);
            RAReport->setTextAlignment(Qt::AlignHCenter);
            alignTable->setItem(currentRow, 0, RAReport);

            QTableWidgetItem *DECReport = new QTableWidgetItem();
            DECReport->setText(dec_report);
            DECReport->setTextAlignment(Qt::AlignHCenter);
            alignTable->setItem(currentRow, 1, DECReport);

            QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
            ObjNameReport->setText(o->longname());
            ObjNameReport->setTextAlignment(Qt::AlignHCenter);
            alignTable->setItem(currentRow, 2, ObjNameReport);

            QTableWidgetItem *disabledBox = new QTableWidgetItem();
            disabledBox->setFlags(Qt::ItemIsSelectable);
            alignTable->setItem(currentRow, 3, disabledBox);
        }
    }
    if (previewShowing)
        updatePreviewAlignPoints();
}

void MountModel::resetAlignmentProcedure()
{
    alignTable->setCellWidget(currentAlignmentPoint, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setFlags(Qt::ItemIsSelectable);
    statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
    alignTable->setItem(currentAlignmentPoint, 3, statusReport);

    emit newLog(i18n("The Mount Model Tool is Reset."));
    startAlignB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    m_IsRunning     = false;
    currentAlignmentPoint = 0;
    abort();
}

bool MountModel::alignmentPointsAreBad()
{
    for (int i = 0; i < alignTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell = alignTable->item(i, 0);
        if (!raCell)
            return true;
        QString raString = raCell->text();
        if (dms().setFromString(raString, false) == false)
            return true;

        QTableWidgetItem *decCell = alignTable->item(i, 1);
        if (!decCell)
            return true;
        QString decString = decCell->text();
        if (dms().setFromString(decString, true) == false)
            return true;
    }
    return false;
}

void MountModel::startStopAlignmentProcedure()
{
    if (!m_IsRunning)
    {
        if (alignTable->rowCount() > 0)
        {
            if (alignmentPointsAreBad())
            {
                KSNotification::error(i18n("Please Check the Alignment Points."));
                return;
            }
            if (m_AlignInstance->currentGOTOMode() == Align::GOTO_NOTHING)
            {
                int r = KMessageBox::warningContinueCancel(
                            nullptr,
                            i18n("In the Align Module, \"Nothing\" is Selected for the Solver Action.  This means that the "
                                 "mount model tool will not sync/align your mount but will only report the pointing model "
                                 "errors.  Do you wish to continue?"),
                            i18n("Pointing Model Report Only?"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                            "nothing_selected_warning");
                if (r == KMessageBox::Cancel)
                    return;
            }
            if (currentAlignmentPoint == 0)
            {
                for (int row = 0; row < alignTable->rowCount(); row++)
                {
                    QTableWidgetItem *statusReport = new QTableWidgetItem();
                    statusReport->setIcon(QIcon());
                    alignTable->setItem(row, 3, statusReport);
                }
            }
            startAlignB->setIcon(
                QIcon::fromTheme("media-playback-pause"));
            m_IsRunning = true;
            emit newLog(i18n("The Mount Model Tool is Starting."));
            startAlignmentPoint();
        }
    }
    else
    {
        startAlignB->setIcon(
            QIcon::fromTheme("media-playback-start"));
        alignTable->setCellWidget(currentAlignmentPoint, 3, new QWidget());
        emit newLog(i18n("The Mount Model Tool is Paused."));
        abort();
        m_IsRunning = false;

        QTableWidgetItem *statusReport = new QTableWidgetItem();
        statusReport->setFlags(Qt::ItemIsSelectable);
        statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
        alignTable->setItem(currentAlignmentPoint, 3, statusReport);
    }
}

void MountModel::startAlignmentPoint()
{
    if (m_IsRunning && currentAlignmentPoint >= 0 && currentAlignmentPoint < alignTable->rowCount())
    {
        QTableWidgetItem *raCell = alignTable->item(currentAlignmentPoint, 0);
        QString raString         = raCell->text();
        dms raDMS                = dms::fromString(raString, false);
        double ra                = raDMS.Hours();

        QTableWidgetItem *decCell = alignTable->item(currentAlignmentPoint, 1);
        QString decString         = decCell->text();
        dms decDMS                = dms::fromString(decString, true);
        double dec                = decDMS.Degrees();

        QProgressIndicator *alignIndicator = new QProgressIndicator(this);
        alignTable->setCellWidget(currentAlignmentPoint, 3, alignIndicator);
        alignIndicator->startAnimation();

        m_AlignInstance->setTargetCoords(ra, dec);
        m_AlignInstance->Slew();
    }
}

void MountModel::finishAlignmentPoint(bool solverSucceeded)
{
    if (m_IsRunning && currentAlignmentPoint >= 0 && currentAlignmentPoint < alignTable->rowCount())
    {
        alignTable->setCellWidget(currentAlignmentPoint, 3, new QWidget());
        QTableWidgetItem *statusReport = new QTableWidgetItem();
        statusReport->setFlags(Qt::ItemIsSelectable);
        if (solverSucceeded)
            statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
        else
            statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
        alignTable->setItem(currentAlignmentPoint, 3, statusReport);

        currentAlignmentPoint++;

        if (currentAlignmentPoint < alignTable->rowCount())
        {
            startAlignmentPoint();
        }
        else
        {
            m_IsRunning = false;
            startAlignB->setIcon(
                QIcon::fromTheme("media-playback-start"));
            emit newLog(i18n("The Mount Model Tool is Finished."));
            currentAlignmentPoint = 0;
        }
    }
}

void MountModel::setAlignStatus(Ekos::AlignState state)
{
    switch (state)
    {
        case ALIGN_COMPLETE:
            if (m_IsRunning)
                finishAlignmentPoint(true);
            break;

        case ALIGN_FAILED:
            if (m_IsRunning)
                finishAlignmentPoint(false);
        default:
            break;
    }
}
}
