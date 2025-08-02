/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequenceeditor.h"

#include "camera.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include <kstars_debug.h>

// These strings are used to store information in the optical train
// for later use in the stand-alone esq editor.
#define KEY_FILTERS     "filtersList"
#define KEY_FORMATS     "formatsList"
#define KEY_ISOS        "isoList"
#define KEY_INDEX       "isoIndex"
#define KEY_H           "captureFrameHN"
#define KEY_W           "captureFrameWN"
#define KEY_GAIN_KWD    "ccdGainKeyword"
#define KEY_OFFSET_KWD  "ccdOffsetKeyword"
#define KEY_TEMPERATURE "ccdTemperatures"
#define KEY_TIMESTAMP   "timestamp"

namespace
{

// Columns in the job table
// Adds the items to the QComboBox if they're not there already.
void addToCombo(QComboBox *combo, const QStringList &items)
{
    if (items.size() == 0)
        return;
    QStringList existingItems;
    for (int index = 0; index < combo->count(); index++)
        existingItems << combo->itemText(index);

    for (const auto &item : items)
        if (existingItems.indexOf(item) == -1)
            combo->addItem(item);
}

} // namespace
namespace Ekos
{

SequenceEditor::SequenceEditor(QWidget * parent) : QDialog(parent)
{
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    setupUi(this);

    // initialize standalone camera
    m_camera.reset(new Camera(true));
    initStandAlone();
    sequenceEditorLayout->insertWidget(1, m_camera.get());
}
void SequenceEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    onStandAloneShow();
    m_camera->onStandAloneShow(event);
}

bool SequenceEditor::loadSequenceQueue(const QString &fileURL, QString targetName)
{
    if (m_camera.isNull())
        return false;
    else
        return m_camera->loadSequenceQueue(fileURL, targetName);
}

void SequenceEditor::initStandAlone()
{
    QSharedPointer<Camera> mainCam = m_camera;
    mainCam->processGrid->setVisible(false);
    mainCam->loadSaveBox->setVisible(true);
    mainCam->loadSaveBox->setEnabled(true);
    mainCam->horizontalSpacer_SQ2->changeSize(0, 0);

    QList<QWidget*> unusedWidgets =
    {
        mainCam->opticalTrainCombo, mainCam->trainB, mainCam->restartCameraB, mainCam->clearConfigurationB, mainCam->resetFrameB, mainCam->opticalTrainLabel,
        mainCam->coolerOnB, mainCam->coolerOffB, mainCam->setTemperatureB, mainCam->temperatureRegulationB, mainCam->temperatureOUT,
        mainCam->previewB, mainCam->loopB, mainCam->liveVideoB, mainCam->startB, mainCam->pauseB,
        mainCam->previewLabel, mainCam->loopLabel, mainCam->videoLabel,
        mainCam->resetB,  mainCam->queueLoadB, mainCam->queueSaveB, mainCam->queueSaveAsB,
        mainCam->darkB, mainCam->darkLibraryB, mainCam->darksLibraryLabel, mainCam->exposureCalcB, mainCam->exposureCalculationLabel,
        mainCam->filterManagerB
    };
    for (auto &widget : unusedWidgets)
    {
        widget->setEnabled(false);
        widget->setVisible(false);
    }
}

void SequenceEditor::onStandAloneShow()
{
    OpticalTrainSettings::Instance()->setOpticalTrainID(Options::captureTrainID());
    auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Capture);
    m_Settings = settings.toJsonObject().toVariantMap();

    QSharedPointer<FilterManager> fm;

    // Default comment if there is no previously saved stand-alone parameters.
    QString comment = i18n("<b><font color=\"red\">Please run the Capture tab connected to INDI with your desired "
                           "camera/filterbank at least once before using the Sequence Editor. </font></b><p>");

    if (m_Settings.contains(KEY_TIMESTAMP) && m_Settings[KEY_TIMESTAMP].toString().size() > 0)
        comment = i18n("<b>Using camera and filterwheel attributes from Capture session started at %1.</b>"
                       "<p>If you wish to use other cameras/filterbanks, please edit the sequence "
                       "using the Capture tab.<br>It is not recommended to overwrite a sequence file currently running, "
                       "please rename it instead.</p><p>", m_Settings[KEY_TIMESTAMP].toString());
    sequenceEditorComment->setVisible(true);
    sequenceEditorComment->setEnabled(true);
    sequenceEditorComment->setStyleSheet("{color: #C0BBFE}");
    sequenceEditorComment->setText(comment);

    // Add extra load and save buttons at the bottom of the window.
    m_camera->loadSaveBox->setEnabled(true);
    m_camera->loadSaveBox->setVisible(true);
    connect(m_camera->esqSaveAsB, &QPushButton::clicked, m_camera.get(),
            &Camera::saveSequenceQueueAs);
    connect(m_camera->esqLoadB, &QPushButton::clicked, m_camera.get(),
            static_cast<void(Camera::*)()>(&Camera::loadSequenceQueue));

    m_camera->FilterPosCombo->clear();
    if (m_Settings.contains(KEY_FILTERS))
        addToCombo(m_camera->FilterPosCombo, m_Settings[KEY_FILTERS].toStringList());

    if (m_camera->FilterPosCombo->count() > 0)
    {
        m_camera->filterEditB->setEnabled(true);
        m_camera->filterManagerB->setEnabled(true);
    }

    m_camera->captureGainN->setEnabled(true);
    m_camera->captureGainN->setSpecialValueText(i18n("--"));

    m_camera->captureOffsetN->setEnabled(true);
    m_camera->captureOffsetN->setSpecialValueText(i18n("--"));

    // Always add these strings to the types menu. Might also add other ones
    // that were used in the last capture session.
    const QStringList frameTypes = {"Light", "Dark", "Bias", "Flat"};
    m_camera->captureTypeS->blockSignals(true);
    m_camera->captureTypeS->clear();
    m_camera->captureTypeS->addItems(frameTypes);
    m_camera->captureTypeS->blockSignals(false);

    // Always add these strings to the encodings menu. Might also add other ones
    // that were used in the last capture session.
    const QStringList frameEncodings = {"FITS", "Native", "XISF"};
    m_camera->captureEncodingS->clear();
    m_camera->captureEncodingS->addItems(frameEncodings);

    if (m_Settings.contains(KEY_FORMATS))
    {
        m_camera->captureFormatS->clear();
        addToCombo(m_camera->captureFormatS, m_Settings[KEY_FORMATS].toStringList());
    }

    m_camera->cameraTemperatureN->setEnabled(true);
    m_camera->cameraTemperatureN->setReadOnly(false);
    m_camera->cameraTemperatureN->setSingleStep(1);
    m_camera->cameraTemperatureS->setEnabled(true);

    double minTemp = -50, maxTemp = 50;
    if (m_Settings.contains(KEY_TEMPERATURE))
    {
        QStringList temperatureList = m_Settings[KEY_TEMPERATURE].toStringList();
        if (temperatureList.size() > 1)
        {
            minTemp = temperatureList[0].toDouble();
            maxTemp = temperatureList[1].toDouble();
        }
    }
    m_camera->cameraTemperatureN->setMinimum(minTemp);
    m_camera->cameraTemperatureN->setMaximum(maxTemp);

    // No pre-configured ISOs are available--would be too much of a guess, but
    // we will use ISOs from the last live capture session.

    if (m_Settings.contains(KEY_ISOS))
    {
        QStringList isoList = m_Settings[KEY_ISOS].toStringList();
        m_camera->captureISOS->clear();
        if (isoList.size() > 0)
        {
            m_camera->captureISOS->addItems(isoList);
            if (m_Settings.contains(KEY_INDEX))
                m_camera->captureISOS->setCurrentIndex(m_Settings[KEY_INDEX].toString().toInt());
            else
                m_camera->captureISOS->setCurrentIndex(0);
            m_camera->captureISOS->blockSignals(false);
            m_camera->captureISOS->setEnabled(true);
        }
    }
    else
    {
        m_camera->captureISOS->blockSignals(true);
        m_camera->captureISOS->clear();
        m_camera->captureISOS->setEnabled(false);
    }

    // Remember the sensor width and height from the last live session.
    // The user can always edit the input box.
    constexpr int maxFrame = 20000;
    m_camera->captureFrameXN->setMaximum(static_cast<int>(maxFrame));
    m_camera->captureFrameYN->setMaximum(static_cast<int>(maxFrame));
    m_camera->captureFrameWN->setMaximum(static_cast<int>(maxFrame));
    m_camera->captureFrameHN->setMaximum(static_cast<int>(maxFrame));

    if (m_Settings.contains(KEY_H))
        m_camera->captureFrameHN->setValue(m_Settings[KEY_H].toUInt());

    if (m_Settings.contains(KEY_W))
        m_camera->captureFrameWN->setValue(m_Settings[KEY_W].toUInt());
}

}
