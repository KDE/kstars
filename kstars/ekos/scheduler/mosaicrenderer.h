/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QGraphicsScene>

class Projector;

namespace Ekos
{
class MosaicTilesManager;
class MosaicRenderer : public QObject
{
        Q_OBJECT
    public:
        explicit MosaicRenderer();
        bool render(uint16_t w, uint16_t h, QImage *image, const Projector *m_proj);

    signals:

    public slots:

    private:

      Projector *m_Projecter {nullptr};
      MosaicTilesManager *m_TilesManager {nullptr};

      QGraphicsScene m_TilesScene;
};
}
