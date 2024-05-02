/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequenceeditor.h"

#include "capture.h"
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
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    setupUi(this);

    // initialize capture standalone
    m_capture.reset(new Capture(true));
    initStandAlone();
    sequenceEditorLayout->insertWidget(1, m_capture.get());
}
void SequenceEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    onStandAloneShow();
    m_capture->onStandAloneShow(event);
}

void SequenceEditor::initStandAlone()
{
    m_capture->cameraUI->processGrid->setVisible(false);
    m_capture->cameraUI->loadSaveBox->setVisible(true);
    m_capture->cameraUI->loadSaveBox->setEnabled(true);
    m_capture->cameraUI->horizontalSpacer_SQ2->changeSize(0, 0);

    QList<QWidget*> unusedWidgets =
    {
        m_capture->cameraUI->opticalTrainCombo, m_capture->cameraUI->trainB, m_capture->cameraUI->restartCameraB, m_capture->cameraUI->clearConfigurationB, m_capture->cameraUI->resetFrameB, m_capture->cameraUI->opticalTrainLabel,
        m_capture->cameraUI->coolerOnB, m_capture->cameraUI->coolerOffB, m_capture->cameraUI->setTemperatureB, m_capture->cameraUI->temperatureRegulationB, m_capture->cameraUI->temperatureOUT,
        m_capture->cameraUI->previewB, m_capture->cameraUI->loopB, m_capture->cameraUI->liveVideoB, m_capture->cameraUI->startB, m_capture->cameraUI->pauseB,
        m_capture->cameraUI->previewLabel, m_capture->cameraUI->loopLabel, m_capture->cameraUI->videoLabel,
        m_capture->cameraUI->resetB,  m_capture->cameraUI->queueLoadB, m_capture->cameraUI->queueSaveB, m_capture->cameraUI->queueSaveAsB,
        m_capture->cameraUI->darkB, m_capture->cameraUI->darkLibraryB, m_capture->cameraUI->darksLibraryLabel, m_capture->cameraUI->exposureCalcB, m_capture->cameraUI->exposureCalculationLabel,
        m_capture->cameraUI->filterManagerB
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
    m_capture->cameraUI->loadSaveBox->setEnabled(true);
    m_capture->cameraUI->loadSaveBox->setVisible(true);
    connect(m_capture->cameraUI->esqSaveAsB, &QPushButton::clicked, m_capture.get(), &Capture::saveSequenceQueueAs);
    connect(m_capture->cameraUI->esqLoadB, &QPushButton::clicked, m_capture.get(),
            static_cast<void(Capture::*)()>(&Capture::loadSequenceQueue));

    m_capture->cameraUI->FilterPosCombo->clear();
    if (m_Settings.contains(KEY_FILTERS))
        addToCombo(m_capture->cameraUI->FilterPosCombo, m_Settings[KEY_FILTERS].toStringList());

    if (m_capture->cameraUI->FilterPosCombo->count() > 0)
    {
        m_capture->cameraUI->filterEditB->setEnabled(true);
        m_capture->cameraUI->filterManagerB->setEnabled(true);
    }

    m_capture->cameraUI->captureGainN->setEnabled(true);
    m_capture->cameraUI->captureGainN->setSpecialValueText(i18n("--"));

    m_capture->cameraUI->captureOffsetN->setEnabled(true);
    m_capture->cameraUI->captureOffsetN->setSpecialValueText(i18n("--"));

    // Always add these strings to the types menu. Might also add other ones
    // that were used in the last capture session.
    const QStringList frameTypes = {"Light", "Dark", "Bias", "Flat"};
    m_capture->cameraUI->captureTypeS->clear();
    m_capture->cameraUI->captureTypeS->addItems(frameTypes);

    // Always add these strings to the encodings menu. Might also add other ones
    // that were used in the last capture session.
    const QStringList frameEncodings = {"FITS", "Native", "XISF"};
    m_capture->cameraUI->captureEncodingS->clear();
    m_capture->cameraUI->captureEncodingS->addItems(frameEncodings);

    if (m_Settings.contains(KEY_FORMATS))
    {
        m_capture->cameraUI->captureFormatS->clear();
        addToCombo(m_capture->cameraUI->captureFormatS, m_Settings[KEY_FORMATS].toStringList());
    }

    m_capture->cameraUI->cameraTemperatureN->setEnabled(true);
    m_capture->cameraUI->cameraTemperatureN->setReadOnly(false);
    m_capture->cameraUI->cameraTemperatureN->setSingleStep(1);
    m_capture->cameraUI->cameraTemperatureS->setEnabled(true);

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
    m_capture->cameraUI->cameraTemperatureN->setMinimum(minTemp);
    m_capture->cameraUI->cameraTemperatureN->setMaximum(maxTemp);

    // No pre-configured ISOs are available--would be too much of a guess, but
    // we will use ISOs from the last live capture session.

    if (m_Settings.contains(KEY_ISOS))
    {
        QStringList isoList = m_Settings[KEY_ISOS].toStringList();
        m_capture->cameraUI->captureISOS->clear();
        if (isoList.size() > 0)
        {
            m_capture->cameraUI->captureISOS->addItems(isoList);
            if (m_Settings.contains(KEY_INDEX))
                m_capture->cameraUI->captureISOS->setCurrentIndex(m_Settings[KEY_INDEX].toString().toInt());
            else
                m_capture->cameraUI->captureISOS->setCurrentIndex(0);
            m_capture->cameraUI->captureISOS->blockSignals(false);
            m_capture->cameraUI->captureISOS->setEnabled(true);
        }
    }
    else
    {
        m_capture->cameraUI->captureISOS->blockSignals(true);
        m_capture->cameraUI->captureISOS->clear();
        m_capture->cameraUI->captureISOS->setEnabled(false);
    }

    // Remember the sensor width and height from the last live session.
    // The user can always edit the input box.
    constexpr int maxFrame = 20000;
    m_capture->cameraUI->captureFrameXN->setMaximum(static_cast<int>(maxFrame));
    m_capture->cameraUI->captureFrameYN->setMaximum(static_cast<int>(maxFrame));
    m_capture->cameraUI->captureFrameWN->setMaximum(static_cast<int>(maxFrame));
    m_capture->cameraUI->captureFrameHN->setMaximum(static_cast<int>(maxFrame));

    if (m_Settings.contains(KEY_H))
        m_capture->cameraUI->captureFrameHN->setValue(m_Settings[KEY_H].toUInt());

    if (m_Settings.contains(KEY_W))
        m_capture->cameraUI->captureFrameWN->setValue(m_Settings[KEY_W].toUInt());
}

}
