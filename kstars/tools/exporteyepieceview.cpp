/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "exporteyepieceview.h"

#include "dms.h"
#include "eyepiecefield.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skypoint.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWidget>

ExportEyepieceView::ExportEyepieceView(const SkyPoint *_sp, const KStarsDateTime &dt, const QPixmap *renderImage,
                                       const QPixmap *renderChart, QWidget *parent)
    : QDialog(parent), m_dt(dt)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    m_sp.reset(new SkyPoint(*_sp)); // Work on a copy.

    Q_ASSERT(renderChart);
    m_renderChart.reset(new QPixmap(*renderChart));

    if (renderImage != nullptr)
        m_renderImage.reset(new QPixmap(*renderImage));

    setWindowTitle(i18nc("@title:window", "Export eyepiece view"));

    QWidget *mainWidget     = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mainWidget);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(slotCloseDialog()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotSaveImage()));

    QVBoxLayout *rows = new QVBoxLayout;
    mainWidget->setLayout(rows);

    QLabel *tickInfoLabel = new QLabel(i18n("Overlay orientation vs. time ticks: "), this);
    m_tickConfigCombo     = new QComboBox(this);
    m_tickConfigCombo->addItem(i18n("None"));
    m_tickConfigCombo->addItem(i18n("Towards Zenith"));
    m_tickConfigCombo->addItem(i18n("Dobsonian View"));

    QHBoxLayout *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget(tickInfoLabel);
    optionsLayout->addWidget(m_tickConfigCombo);
    optionsLayout->addStretch();
    rows->addLayout(optionsLayout);

    m_tickWarningLabel = new QLabel(this);
    rows->addWidget(m_tickWarningLabel);

    m_outputDisplay = new QLabel;
    m_outputDisplay->setBackgroundRole(QPalette::Base);
    m_outputDisplay->setScaledContents(false);
    m_outputDisplay->setMinimumWidth(400);
    m_outputDisplay->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rows->addWidget(m_outputDisplay);

    connect(m_tickConfigCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotOverlayTicks(int)));
    connect(m_tickConfigCombo, SIGNAL(activated(int)), this, SLOT(slotOverlayTicks(int)));

    render();
    show();
}

void ExportEyepieceView::slotOverlayTicks(int tickConfig)
{
    m_tickConfig = tickConfig;
    if (tickConfig == 0)
        m_tickWarningLabel->setText(QString());
    else if (tickConfig == 1)
        m_tickWarningLabel->setText(i18n("Note: This overlay makes sense only if the view was generated in alt/az mode "
                                         "with a preset such as Refractor or Vanilla"));
    else if (tickConfig == 2)
        m_tickWarningLabel->setText(i18n("Note: This overlay  makes sense only if the view was generated in alt/az "
                                         "mode with a preset such as Dobsonian"));
    render();
}

void ExportEyepieceView::render()
{
    float baseWidth  = m_renderChart->width();
    float baseHeight = m_renderChart->height();

    if (m_renderImage.get() != nullptr)
        m_output = QImage(int(baseWidth * 2.25), int(baseHeight),
                          QImage::Format_ARGB32); // 0.25 * baseWidth gap between the images
    else
        m_output = QImage(int(baseWidth), int(baseHeight), QImage::Format_ARGB32);

    m_output.fill(Qt::white);
    QPainter op(&m_output);
    op.drawPixmap(QPointF(0, 0), *m_renderChart);
    if (m_renderImage.get() != nullptr)
        op.drawPixmap(QPointF(baseWidth * 1.25, 0), *m_renderImage);

    if (m_tickConfig != 0 && Options::useAltAz()) // FIXME: this is very skymap-state-heavy for my happiness --asimha
    {
        // we must draw ticks
        QImage tickOverlay(baseWidth, baseHeight, QImage::Format_ARGB32);
        tickOverlay.fill(Qt::transparent);

        QPainter p(&tickOverlay);
        p.setPen(Qt::red); // FIXME: Determine color depending on situation, or make it configurable
        double rEnd   = 0.85 * (baseWidth / 2.);
        double rStart = 0.8 * (baseWidth / 2.);
        QFont font;
        font.setPixelSize((rEnd - rStart));
        p.setFont(font);

        GeoLocation *geo = KStarsData::Instance()->geo();
        double alt0      = m_sp->alt().Degrees(); // altitude when hour = 0, i.e. at m_dt (see below).
        dms northAngle0  = EyepieceField::findNorthAngle(m_sp.get(), geo->lat());

        for (float hour = -3.5; hour <= 3.5; hour += 0.5)
        {
            dms rotation; // rotation

            // FIXME: Code duplication : code duplicated from EyepieceField. This should really be a member of SkyPoint or something.
            SkyPoint sp    = SkyPoint::timeTransformed(m_sp.get(), m_dt, geo, hour);
            double alt     = sp.alt().Degrees();
            dms northAngle = EyepieceField::findNorthAngle(&sp, geo->lat());

            rotation = (northAngle - northAngle0);
            if (m_tickConfig == 2)
            {
                // Dobsonian: add additional CW rotation by altitude, but compensate for the fact that we've already rotated by alt0
                rotation = rotation - dms((alt - alt0));
            }
            rotation = rotation.reduce();
            p.save();
            p.translate(baseWidth / 2.0, baseHeight / 2.0);
            p.rotate(-(rotation.Degrees() + 90.0));
            p.drawLine(QPointF(rStart, 0), QPointF(rEnd, 0));
            QTime ct = geo->UTtoLT(m_dt.addSecs(3600.0 * hour)).time();
            p.drawText(QPointF(rEnd + 0.01 * baseWidth, 0), QString::asprintf("%02d:%02d", ct.hour(), ct.minute()));
            p.restore();
        }
        p.end();
        op.drawImage(QPointF(0, 0), tickOverlay);
        if (m_renderImage.get() != nullptr)
        {
            op.drawImage(QPointF(baseWidth * 1.25, 0), tickOverlay);
        }
    }
    op.end();

    m_outputDisplay->setPixmap((QPixmap::fromImage(m_output))
                                   .scaled(m_outputDisplay->width(), m_outputDisplay->height(), Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
}

void ExportEyepieceView::slotSaveImage()
{
    // does nothing at the moment. TODO: Implement.
    QString fileName = QFileDialog::getSaveFileName(this, i18nc("@title:window", "Save Image as"), QString(),
                                                    i18n("Image files (*.png *.jpg *.xpm *.bmp *.gif)"));
    if (!fileName.isEmpty())
    {
        m_output.save(fileName);
        slotCloseDialog();
    }
}

void ExportEyepieceView::slotCloseDialog()
{
    hide();
    deleteLater();
}
