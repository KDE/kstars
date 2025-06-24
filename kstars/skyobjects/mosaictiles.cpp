/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QPainter>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "mosaictiles.h"
#include "kstarsdata.h"
#include "Options.h"

MosaicTiles::MosaicTiles() : SkyObject()
{
    setName(QLatin1String("Mosaic Tiles"));

    m_Brush.setStyle(Qt::NoBrush);
    m_Pen.setColor(QColor(200, 200, 200, 100));
    m_Pen.setWidth(1);

    m_TextBrush.setStyle(Qt::SolidPattern);
    m_TextPen.setColor(Qt::red);
    m_TextPen.setWidth(2);

    m_FocalLength = Options::telescopeFocalLength();
    m_FocalReducer = Options::telescopeFocalReducer();
    m_CameraSize.setWidth(Options::cameraWidth());
    m_CameraSize.setHeight(Options::cameraHeight());
    m_PixelSize.setWidth(Options::cameraPixelWidth());
    m_PixelSize.setHeight(Options::cameraPixelHeight());
    m_PositionAngle = Options::cameraRotation();

    // Initially for 1x1 Grid
    m_MosaicFOV = m_CameraFOV = calculateCameraFOV();

    createTiles(false);
}

MosaicTiles::~MosaicTiles()
{
}

bool MosaicTiles::isValid() const
{
    return m_MosaicFOV.width() > 0 && m_MosaicFOV.height() > 0;
}

void MosaicTiles::setPositionAngle(double value)
{
    m_PositionAngle = value;
}

void MosaicTiles::setOverlap(double value)
{
    m_Overlap = (value < 0) ? 0 : (value > 100) ? 100 : value;
}

std::shared_ptr<MosaicTiles::OneTile> MosaicTiles::oneTile(int row, int col)
{
    int offset = row * m_GridSize.width() + col;

    if (offset < 0 || offset >= m_Tiles.size())
        return nullptr;

    return m_Tiles[offset];
}

bool MosaicTiles::fromXML(const QString &filename)
{
    QFile sFile;
    sFile.setFileName(filename);

    if (!sFile.open(QIODevice::ReadOnly))
        return false;

    LilXML *xmlParser = newLilXML();
    char errmsg[2048] = {0};
    XMLEle *root = nullptr;
    XMLEle *ep   = nullptr;
    char c;

    m_OperationMode = MODE_OPERATION;

    m_Tiles.clear();

    // We expect all data read from the XML to be in the C locale - QLocale::c()
    QLocale cLocale = QLocale::c();

    bool mosaicInfoFound = false;
    int index = 1;

    m_TrackChecked = m_FocusChecked = m_AlignChecked = m_GuideChecked = false;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *tag = tagXMLEle(ep);
                if (!strcmp(tag, "Mosaic"))
                {
                    mosaicInfoFound = true;
                    XMLEle *subEP = nullptr;
                    for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
                    {
                        const char *subTag = tagXMLEle(subEP);
                        if (!strcmp(subTag, "Target"))
                            setTargetName(pcdataXMLEle(subEP));
                        else if (!strcmp(subTag, "Group"))
                            setGroup(pcdataXMLEle(subEP));
                        else if (!strcmp(subTag, "FinishSequence"))
                            setCompletionCondition(subTag);
                        else if (!strcmp(subTag, "FinishRepeat"))
                            setCompletionCondition(subTag, pcdataXMLEle(subEP));
                        else if (!strcmp(subTag, "FinishSLoop"))
                            setCompletionCondition(subTag);
                        else if (!strcmp(subTag, "Sequence"))
                            setSequenceFile(pcdataXMLEle(subEP));
                        else if (!strcmp(subTag, "Directory"))
                            setOutputDirectory(pcdataXMLEle(subEP));
                        else if (!strcmp(subTag,  "FocusEveryN"))
                            setFocusEveryN(cLocale.toInt(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "AlignEveryN"))
                            setAlignEveryN(cLocale.toInt(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag,  "TrackChecked"))
                            m_TrackChecked = true;
                        else if (!strcmp(subTag,  "FocusChecked"))
                            m_FocusChecked = true;
                        else if (!strcmp(subTag, "AlignChecked"))
                            m_AlignChecked = true;
                        else if (!strcmp(subTag, "GuideChecked"))
                            m_GuideChecked = true;
                        else if (!strcmp(subTag, "Overlap"))
                            setOverlap(cLocale.toDouble(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "CenterRA"))
                        {
                            dms ra;
                            ra.setH(cLocale.toDouble(pcdataXMLEle(subEP)));
                            setRA0(ra);
                        }
                        else if (!strcmp(subTag, "CenterDE"))
                        {
                            dms de;
                            de.setD(cLocale.toDouble(pcdataXMLEle(subEP)));
                            setDec0(de);
                        }
                        else if (!strcmp(subTag, "GridW"))
                            m_GridSize.setWidth(cLocale.toInt(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "GridH"))
                            m_GridSize.setHeight(cLocale.toInt(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "FOVW"))
                            m_MosaicFOV.setWidth(cLocale.toDouble(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "FOVH"))
                            m_MosaicFOV.setHeight(cLocale.toDouble(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "CameraFOVW"))
                            m_CameraFOV.setWidth(cLocale.toDouble(pcdataXMLEle(subEP)));
                        else if (!strcmp(subTag, "CameraFOVH"))
                            m_CameraFOV.setHeight(cLocale.toDouble(pcdataXMLEle(subEP)));
                    }
                }
                else if (mosaicInfoFound && !strcmp(tag, "Job"))
                    processJobInfo(ep, index++);
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            delLilXML(xmlParser);
            return false;
        }
    }

    delLilXML(xmlParser);
    if (mosaicInfoFound)
        updateCoordsNow(KStarsData::Instance()->updateNum());

    if (updateCallback)
    {
        QJsonObject tilesJSON;
        toJSON(tilesJSON);
        updateCallback(tilesJSON);
    }

    return mosaicInfoFound;
}

bool MosaicTiles::processJobInfo(XMLEle *root, int index)
{
    XMLEle *ep;
    XMLEle *subEP;

    // We expect all data read from the XML to be in the C locale - QLocale::c()
    QLocale cLocale = QLocale::c();

    OneTile newTile;
    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        newTile.index = index;
        if (!strcmp(tagXMLEle(ep), "Coordinates"))
        {
            subEP = findXMLEle(ep, "J2000RA");
            if (subEP)
            {
                dms ra;
                ra.setH(cLocale.toDouble(pcdataXMLEle(subEP)));
                newTile.skyCenter.setRA0(ra);
            }
            subEP = findXMLEle(ep, "J2000DE");
            if (subEP)
            {
                dms de;
                de.setD(cLocale.toDouble(pcdataXMLEle(subEP)));
                newTile.skyCenter.setDec0(de);
            }
        }
        else if (!strcmp(tagXMLEle(ep), "TileCenter"))
        {
            if ((subEP = findXMLEle(ep, "X")))
                newTile.center.setX(cLocale.toDouble(pcdataXMLEle(subEP)));
            if ((subEP = findXMLEle(ep, "Y")))
                newTile.center.setY(cLocale.toDouble(pcdataXMLEle(subEP)));
            if ((subEP = findXMLEle(ep, "Rotation")))
                newTile.rotation = cLocale.toDouble(pcdataXMLEle(subEP));
        }
        else if (!strcmp(tagXMLEle(ep), "PositionAngle"))
        {
            m_PositionAngle = cLocale.toDouble(pcdataXMLEle(ep));
        }
    }

    newTile.skyCenter.updateCoordsNow(KStarsData::Instance()->updateNum());
    appendTile(newTile);
    return true;
}


bool MosaicTiles::toJSON(QJsonObject &output)
{
    output["focalLength"] = m_FocalLength;
    output["focalReducer"] = m_FocalReducer;
    output["positionAngle"] = m_PositionAngle;

    QJsonObject cameraSizeObj;
    cameraSizeObj["width"] = m_CameraSize.width();
    cameraSizeObj["height"] = m_CameraSize.height();
    output["cameraSize"] = cameraSizeObj;

    QJsonObject pixelSizeObj;
    pixelSizeObj["width"] = m_PixelSize.width();
    pixelSizeObj["height"] = m_PixelSize.height();
    output["pixelSize"] = pixelSizeObj;

    QJsonObject gridSizeObj;
    gridSizeObj["width"] = m_GridSize.width();
    gridSizeObj["height"] = m_GridSize.height();
    output["gridSize"] = gridSizeObj;

    output["overlap"] = m_Overlap;

    QJsonObject cameraFOVObj;
    cameraFOVObj["width"] = m_CameraFOV.width();
    cameraFOVObj["height"] = m_CameraFOV.height();
    output["cameraFOV"] = cameraFOVObj;

    QJsonObject mosaicFOVObj;
    mosaicFOVObj["width"] = m_MosaicFOV.width();
    mosaicFOVObj["height"] = m_MosaicFOV.height();
    output["mosaicFOV"] = mosaicFOVObj;

    output["painterAlpha"] = m_PainterAlpha;
    output["painterAlphaAuto"] = m_PainterAlphaAuto;
    output["targetName"] = m_TargetName;
    output["group"] = m_Group;
    output["completionCondition"] = m_CompletionCondition;
    output["completionConditionArg"] = m_CompletionConditionArg;
    output["sequenceFile"] = m_SequenceFile;
    output["outputDirectory"] = m_OutputDirectory;
    output["focusEveryN"] = m_FocusEveryN;
    output["alignEveryN"] = m_AlignEveryN;
    output["isTrackChecked"] = m_TrackChecked;
    output["isFocusChecked"] = m_FocusChecked;
    output["isAlignChecked"] = m_AlignChecked;
    output["isGuideChecked"] = m_GuideChecked;

    output["ra0"] = ra0().Degrees();
    output["dec0"] = dec0().Degrees();
    output["ra"] = ra().Degrees();
    output["dec"] = dec().Degrees();

    QJsonArray tilesArray;
    for (const auto &tile : m_Tiles)
    {
        QJsonObject tileObj;

        QJsonObject posObj;
        posObj["x"] = tile->pos.x();
        posObj["y"] = tile->pos.y();
        tileObj["pos"] = posObj;

        QJsonObject centerObj;
        centerObj["x"] = tile->center.x();
        centerObj["y"] = tile->center.y();
        tileObj["center"] = centerObj;

        QJsonObject skyCenterObj;
        skyCenterObj["ra0"] = tile->skyCenter.ra0().Degrees();
        skyCenterObj["dec0"] = tile->skyCenter.dec0().Degrees();
        skyCenterObj["ra"] = tile->skyCenter.ra().Degrees();
        skyCenterObj["dec"] = tile->skyCenter.dec().Degrees();
        tileObj["skyCenter"] = skyCenterObj;

        tileObj["rotation"] = tile->rotation;
        tileObj["index"] = tile->index;

        tilesArray.append(tileObj);
    }
    output["tiles"] = tilesArray;

    return true;
}

//bool MosaicTiles::fromJSON(const QJsonObject &input)
//{
//    Q_UNUSED(input)
//    return false;
//}

void MosaicTiles::appendTile(const OneTile &value)
{
    m_Tiles.append(std::make_shared<OneTile>(value));
}

void MosaicTiles::appendEmptyTile()
{
    m_Tiles.append(std::make_shared<OneTile>());
}

void MosaicTiles::clearTiles()
{
    m_Tiles.clear();
}

QSizeF MosaicTiles::adjustCoordinate(QPointF tileCoord)
{
    // Compute the declination of the tile row from the mosaic center
    double const dec = dec0().Degrees() + tileCoord.y() / 60.0;

    // Adjust RA based on the shift in declination
    QSizeF const toSpherical(1 / cos(dec * dms::DegToRad), 1);

    // Return the adjusted coordinates as a QSizeF in degrees
    return QSizeF(tileCoord.x() / 60.0 * toSpherical.width(), tileCoord.y() / 60.0 * toSpherical.height());
}

void MosaicTiles::createTiles(bool s_shaped)
{
    m_SShaped = s_shaped;
    updateTiles();
}

void MosaicTiles::updateTiles()
{
    // Sky map has objects moving from left to right, so configure the mosaic from right to left, column per column
    const auto fovW = m_CameraFOV.width();
    const auto fovH = m_CameraFOV.height();
    const auto gridW = m_GridSize.width();
    const auto gridH = m_GridSize.height();

    // Offset is our tile size with an overlap removed
    double const xOffset = fovW * (1 - m_Overlap / 100.0);
    double const yOffset = fovH * (1 - m_Overlap / 100.0);

    // We start at top right corner, (0,0) being the center of the tileset
    double initX = +(fovW + xOffset * (gridW - 1)) / 2.0 - fovW;
    double initY = -(fovH + yOffset * (gridH - 1)) / 2.0;

    double x = initX, y = initY;

    //    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile FovW" << fovW << "FovH" << fovH << "initX" << x << "initY" << y <<
    //                                   "Offset X " << xOffset << " Y " << yOffset << " rotation " << pa << " reverseOdd " << s_shaped;

    // Start by clearing existing tiles.
    clearTiles();

    int index = 0;
    for (int col = 0; col < gridW; col++)
    {
        y = (m_SShaped && (col % 2)) ? (y - yOffset) : initY;

        for (int row = 0; row < gridH; row++)
        {
            QPointF pos(x, y);
            QPointF tile_center(pos.x() + (fovW / 2.0), pos.y() + (fovH / 2.0));

            // The location of the tile on the sky map refers to the center of the mosaic, and rotates with the mosaic itself
            const auto tileSkyLocation = QPointF(0, 0) - rotatePoint(tile_center, QPointF(), m_PositionAngle);

            // Compute the adjusted location in RA/DEC
            const auto tileSkyOffsetScaled = adjustCoordinate(tileSkyLocation);

            auto adjusted_ra0 = (ra0().Degrees() + tileSkyOffsetScaled.width()) / 15.0;
            auto adjusted_de0 = (dec0().Degrees() + tileSkyOffsetScaled.height());
            SkyPoint sky_center(adjusted_ra0, adjusted_de0);
            sky_center.apparentCoord(static_cast<long double>(J2000), KStarsData::Instance()->ut().djd());

            auto tile_center_ra0 = sky_center.ra0().Degrees();
            auto mosaic_center_ra0 = ra0().Degrees();
            auto rotation =  tile_center_ra0 - mosaic_center_ra0;

            // Large rotations handled wrong by the algorithm - prefer doing multiple mosaics
            if (abs(rotation) <= 90.0)
            {
                auto next_index = ++index;
                MosaicTiles::OneTile tile = {pos, tile_center, sky_center, rotation, next_index};
                appendTile(tile);
            }
            else
            {
                appendEmptyTile();
            }

            y += (m_SShaped && (col % 2)) ? -yOffset : +yOffset;
        }

        x -= xOffset;
    }

    if (updateCallback)
    {
        QJsonObject tilesJSON;
        toJSON(tilesJSON);
        updateCallback(tilesJSON);
    }
}

void MosaicTiles::draw(QPainter *painter)
{
    if (m_Tiles.size() == 0)
        return;

    auto pixelScale = Options::zoomFactor() * dms::DegToRad / 60.0;
    const auto fovW = m_CameraFOV.width() * pixelScale;
    const auto fovH = m_CameraFOV.height() * pixelScale;
    const auto mosaicFOVW = m_MosaicFOV.width() * pixelScale;
    const auto mosaicFOVH = m_MosaicFOV.height() * pixelScale;
    const auto gridW = m_GridSize.width();
    const auto gridH = m_GridSize.height();

    QFont defaultFont = painter->font();
    QRect const oneRect(-fovW / 2, -fovH / 2, fovW, fovH);

    auto alphaValue = m_PainterAlpha;

    if (m_PainterAlphaAuto)
    {
        // Tiles should be more transparent when many are overlapped
        // Overlap < 50%: low transparency, as only two tiles will overlap on a line
        // 50% < Overlap < 75%: mid transparency, as three tiles will overlap one a line
        // 75% < Overlap: high transparency, as four tiles will overlap on a line
        // Slider controlling transparency provides [5%,50%], which is scaled to 0-200 alpha.

        if (m_Tiles.size() > 1)
            alphaValue = (40 - m_Overlap / 2);
        else
            alphaValue = 40;
    }

    // Draw a light background field first to help detect holes - reduce alpha as we are stacking tiles over this
    painter->setBrush(QBrush(QColor(255, 0, 0, (200 * alphaValue) / 100), Qt::SolidPattern));
    painter->setPen(QPen(painter->brush(), 2, Qt::PenStyle::DotLine));
    painter->drawRect(QRectF(QPointF(-mosaicFOVW / 2, -mosaicFOVH / 2), QSizeF(mosaicFOVW, mosaicFOVH)));

    // Fill tiles with a transparent brush to show overlaps
    QBrush tileBrush(QColor(0, 255, 0, (200 * alphaValue) / 100), Qt::SolidPattern);

    // Draw each tile, adjusted for rotation
    for (int row = 0; row < gridH; row++)
    {
        for (int col = 0; col < gridW; col++)
        {
            auto tile = oneTile(row, col);
            if (tile)
            {
                painter->save();

                painter->translate(tile->center * pixelScale);
                painter->rotate(tile->rotation);

                painter->setBrush(tileBrush);
                painter->setPen(m_Pen);

                painter->drawRect(oneRect);

                painter->restore();
            }
        }
    }

    // Overwrite with tile information
    for (int row = 0; row < gridH; row++)
    {
        for (int col = 0; col < gridW; col++)
        {
            auto tile = oneTile(row, col);
            if (tile)
            {
                painter->save();

                painter->translate(tile->center * pixelScale);
                // Add 180 to match Position Angle per the standard definition
                // when camera image is read bottom-up instead of KStars standard top-bottom.
                //painter->rotate(tile->rotation + 180);

                painter->rotate(tile->rotation);

                painter->setBrush(m_TextBrush);
                painter->setPen(m_TextPen);

                defaultFont.setPointSize(qMax(1., 4 * pixelScale * m_CameraFOV.width() / 60.));
                painter->setFont(defaultFont);
                painter->drawText(oneRect, Qt::AlignRight | Qt::AlignTop, QString("%1.").arg(tile->index));

                defaultFont.setPointSize(qMax(1., 4 * pixelScale * m_CameraFOV.width() / 60.));
                painter->setFont(defaultFont);
                painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1\n%2")
                                  .arg(tile->skyCenter.ra0().toHMSString(), tile->skyCenter.dec0().toDMSString()));
                painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignBottom, QString("%1%2°")
                                  .arg(tile->rotation >= 0.01 ? '+' : tile->rotation <= -0.01 ? '-' : '~')
                                  .arg(abs(tile->rotation), 5, 'f', 2));

                painter->restore();
            }
        }
    }
}

QPointF MosaicTiles::rotatePoint(QPointF pointToRotate, QPointF centerPoint, double paDegrees)
{
    if (paDegrees < 0)
        paDegrees += 360;
    double angleInRadians = -paDegrees * dms::DegToRad;
    double cosTheta       = cos(angleInRadians);
    double sinTheta       = sin(angleInRadians);

    QPointF rotation_point;

    rotation_point.setX((cosTheta * (pointToRotate.x() - centerPoint.x()) -
                         sinTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.x()));
    rotation_point.setY((sinTheta * (pointToRotate.x() - centerPoint.x()) +
                         cosTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.y()));

    return rotation_point;
}

QSizeF MosaicTiles::calculateTargetMosaicFOV() const
{
    const auto xFOV = m_CameraFOV.width() * (1 - m_Overlap / 100.0);
    const auto yFOV = m_CameraFOV.height() * (1 - m_Overlap / 100.0);
    return QSizeF(xFOV, yFOV);
}

QSize MosaicTiles::mosaicFOVToGrid() const
{
    // Else we get one tile, plus as many overlapping camera FOVs in the remnant of the target FOV
    const auto xFOV = m_CameraFOV.width() * (1 - m_Overlap / 100.0);
    const auto yFOV = m_CameraFOV.height() * (1 - m_Overlap / 100.0);
    const auto xTiles = 1 + ceil((m_MosaicFOV.width() - m_CameraFOV.width()) / xFOV);
    const auto yTiles = 1 + ceil((m_MosaicFOV.height() - m_CameraFOV.height()) / yFOV);
    return QSize(xTiles, yTiles);
}

QSizeF MosaicTiles::calculateCameraFOV() const
{
    auto reducedFocalLength = m_FocalLength * m_FocalReducer;
    // Calculate FOV in arcmins
    double const fov_x =
        206264.8062470963552 * m_CameraSize.width() * m_PixelSize.width() / 60000.0 / reducedFocalLength;
    double const fov_y =
        206264.8062470963552 * m_CameraSize.height() * m_PixelSize.height() / 60000.0 / reducedFocalLength;
    return QSizeF(fov_x, fov_y);
}

void MosaicTiles::syncFOVs()
{
    m_CameraFOV = calculateCameraFOV();
    m_MosaicFOV = calculateTargetMosaicFOV();
}
