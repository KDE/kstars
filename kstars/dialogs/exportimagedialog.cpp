/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "exportimagedialog.h"

#include "imageexporter.h"
#include "kstars.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "ksnotification.h"
#include "printing/legend.h"

#include <KMessageBox>

#include <QDesktopWidget>
#include <QDir>
#include <QPushButton>
#include <QtSvg/QSvgGenerator>

ExportImageDialogUI::ExportImageDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

ExportImageDialog::ExportImageDialog(const QString &url, const QSize &size, ImageExporter *imgExporter)
    : QDialog((QWidget *)KStars::Instance()), m_KStars(KStars::Instance()), m_Url(url), m_Size(size)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    m_DialogUI = new ExportImageDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_DialogUI);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(exportImage()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    QPushButton *previewB = new QPushButton(i18n("Preview image"));
    buttonBox->addButton(previewB, QDialogButtonBox::ActionRole);
    connect(previewB, SIGNAL(clicked()), this, SLOT(previewImage()));

    connect(m_DialogUI->addLegendCheckBox, SIGNAL(toggled(bool)), this, SLOT(switchLegendEnabled(bool)));
    connect(m_DialogUI->addLegendCheckBox, SIGNAL(toggled(bool)), previewB, SLOT(setEnabled(bool)));

    m_ImageExporter = ((imgExporter) ? imgExporter : new ImageExporter(this));

    setWindowTitle(i18nc("@title:window", "Export sky image"));

    setupWidgets();
}

void ExportImageDialog::switchLegendEnabled(bool enabled)
{
    m_DialogUI->legendOrientationLabel->setEnabled(enabled);
    m_DialogUI->legendOrientationComboBox->setEnabled(enabled);
    m_DialogUI->legendTypeLabel->setEnabled(enabled);
    m_DialogUI->legendTypeComboBox->setEnabled(enabled);
    m_DialogUI->legendPositionLabel->setEnabled(enabled);
    m_DialogUI->legendPositionComboBox->setEnabled(enabled);
}

void ExportImageDialog::previewImage()
{
    updateLegendSettings();
    const Legend *legend = m_ImageExporter->getLegend();

    // Preview current legend settings on sky map
    m_KStars->map()->setLegend(Legend(*legend));
    m_KStars->map()->setPreviewLegend(true);

    // Update sky map
    m_KStars->map()->forceUpdate(true);

    // Hide export dialog
    hide();
}

void ExportImageDialog::setupWidgets()
{
    m_DialogUI->addLegendCheckBox->setChecked(true);

    m_DialogUI->legendOrientationComboBox->addItem(i18n("Horizontal"));
    m_DialogUI->legendOrientationComboBox->addItem(i18n("Vertical"));

    QStringList types;
    types << i18n("Full legend") << i18n("Scale with magnitudes chart") << i18n("Only scale") << i18n("Only magnitudes")
          << i18n("Only symbols");
    m_DialogUI->legendTypeComboBox->addItems(types);

    QStringList positions;
    positions << i18n("Upper left corner") << i18n("Upper right corner") << i18n("Lower left corner")
              << i18n("Lower right corner");
    m_DialogUI->legendPositionComboBox->addItems(positions);
}

void ExportImageDialog::updateLegendSettings()
{
    Legend::LEGEND_ORIENTATION orientation =
        ((m_DialogUI->legendOrientationComboBox->currentIndex() == 1) ? Legend::LO_VERTICAL : Legend::LO_HORIZONTAL);

    Legend::LEGEND_TYPE type = static_cast<Legend::LEGEND_TYPE>(m_DialogUI->legendTypeComboBox->currentIndex());

    Legend::LEGEND_POSITION pos =
        static_cast<Legend::LEGEND_POSITION>(m_DialogUI->legendPositionComboBox->currentIndex());

    m_ImageExporter->setLegendProperties(type, orientation, pos);
}

void ExportImageDialog::exportImage()
{
    qDebug() << "Exporting sky image";
    updateLegendSettings();
    m_ImageExporter->includeLegend(m_DialogUI->addLegendCheckBox->isChecked());
    if (!m_ImageExporter->exportImage(m_Url))
    {
        KSNotification::sorry(m_ImageExporter->getLastErrorMessage(), i18n("Could not export image"));
    }
}
