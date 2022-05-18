/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QQuickView>
#include <klocalizedcontext.h>
#include <klocalizedstring.h>
#include <QQmlContext>

#include "mosaictilesmodel.h"
#include "Options.h"
#include "ekos_scheduler_debug.h"

namespace Ekos
{

MosaicTilesModel::MosaicTilesModel(QObject *parent) : QObject(parent)
{
    m_FocalLength = Options::telescopeFocalLength();
    m_CameraSize.setWidth(Options::cameraWidth());
    m_CameraSize.setHeight(Options::cameraHeight());
    m_PixelSize.setWidth(Options::cameraPixelWidth());
    m_PixelSize.setHeight(Options::cameraPixelHeight());
    m_PositionAngle = Options::cameraRotation();
}

void MosaicTilesModel::setPositionAngle(double value)
{
    m_PositionAngle = std::fmod(value * -1 + 360.0, 360.0);
}

void MosaicTilesModel::setOverlap(double value)
{
    m_Overlap = (value < 0) ? 0 : (1 < value) ? 1 : value;
}

MosaicTilesModel::~MosaicTilesModel()
{
}

std::shared_ptr<MosaicTilesModel::OneTile> MosaicTilesModel::oneTile(int row, int col)
{
    int offset = row * m_GridSize.width() + col;

    if (offset < 0 || offset >= m_Tiles.size())
        return nullptr;

    return m_Tiles[offset];
}

bool MosaicTilesModel::toXML(const QTextStream &output)
{
    Q_UNUSED(output)
    return false;
}

bool MosaicTilesModel::fromXML(XMLEle *root)
{
    Q_UNUSED(root)
    return false;
}

bool MosaicTilesModel::toJSON(QJsonObject &output)
{
    Q_UNUSED(output)
    return false;
}

bool MosaicTilesModel::fromJSON(const QJsonObject &input)
{
    Q_UNUSED(input)
    return false;
}

void MosaicTilesModel::appendTile(const OneTile &value)
{
    m_Tiles.append(std::make_shared<OneTile>(value));
}

void MosaicTilesModel::appendEmptyTile()
{
    m_Tiles.append(std::make_shared<OneTile>());
}

void MosaicTilesModel::clearTiles()
{
    m_Tiles.clear();
}

}
