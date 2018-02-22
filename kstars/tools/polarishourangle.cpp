/*
  Copyright (C) 2015-2017, Pavel Mraz

  Copyright (C) 2017, Jasem Mutlaq

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "polarishourangle.h"

#include "skyobject.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"

#include <QPainter>
#include <QPen>

float hbase = 24;
float vangle = 15;

PolarisHourAngle::PolarisHourAngle(QWidget *parent) :
  QDialog(parent),
  m_polarisHourAngle(0)
{
  setupUi(this);
  setFixedHeight(size().height());

  SkyObject *polaris = KStarsData::Instance()->skyComposite()->findByName(i18nc("star name", "Polaris"));
  Q_ASSERT_X(polaris != nullptr, "PolarisHourAngle", "Unable to find Polaris!");
  m_polaris = polaris->clone();

  m_reticle12.reset(new QPixmap(":/images/reticle12.png"));
  m_reticle24.reset(new QPixmap(":/images/reticle24.png"));

  connect(dateTimeEdit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(onTimeUpdated(QDateTime)));
  connect(currentTimeB, &QPushButton::clicked, this, [this]() { dateTimeEdit->setDateTime(KStarsData::Instance()->lt()); });
  connect(twelveHourR, SIGNAL(toggled(bool)), this, SLOT(update()));

  dateTimeEdit->setDateTime(KStarsData::Instance()->lt());
}

void PolarisHourAngle::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  QPointF center = frame->rect().center();
  double r1 = 175;

  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  p.drawPixmap(frame->pos(), twelveHourR->isChecked() ? *(m_reticle12.get()) : *(m_reticle24.get()));
  p.setPen(Qt::yellow);
  p.setBrush(Qt::white);
  p.translate(frame->pos());

//  double angle = (24.0 - m_polarisHourAngle) * 15.0;
  double angle = (hbase - m_polarisHourAngle) * vangle;
  p.save();

  p.translate(center);
  p.rotate(angle);

  QPolygon poly;

  p.setPen(QPen(Qt::red, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
  p.setBrush(p.pen().color());

  poly << QPoint(0, r1);
  poly << QPoint(10, -10);
  poly << QPoint(5, -14);
  poly << QPoint(-5, -14);
  poly << QPoint(-10, -10);
  poly << QPoint(0, r1);

  p.drawPolygon(poly);

  p.restore();
}

void PolarisHourAngle::onTimeUpdated(QDateTime newDateTime)
{
    KStarsDateTime lt(newDateTime);
    KSNumbers num(lt.djd());
    m_polaris->updateCoords(&num, false);
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(lt.gst());
/*
    m_polarisHourAngle = (lst.Degrees() - m_polaris->ra().Degrees())/15.0;
    while (m_polarisHourAngle > 24)
        m_polarisHourAngle -= 24;
    while (m_polarisHourAngle < 0)
        m_polarisHourAngle += 24;
*/
    m_polarisHourAngle = (lst.Degrees() - m_polaris->ra().Degrees())/vangle;
    while (m_polarisHourAngle > hbase)
        m_polarisHourAngle -= hbase;
    while (m_polarisHourAngle < hbase)
        m_polarisHourAngle += hbase;

    labelPolarisHA->setText(dms(m_polarisHourAngle*vangle).toHMSString());
    labelDate->setText(newDateTime.date().toString());
    labelTime->setText(newDateTime.time().toString());

    update();
}
