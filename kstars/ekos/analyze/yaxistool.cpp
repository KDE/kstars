/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "yaxistool.h"

#include <QColorDialog>
#include "Options.h"
#include <kstars_debug.h>

YAxisToolUI::YAxisToolUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

YAxisTool::YAxisTool(QWidget *w) : QDialog(w)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui = new YAxisToolUI(this);

    ui->setStyleSheet("QPushButton:checked { background-color: red; }");

    setWindowTitle(i18nc("@title:window", "Y-Axis Tool"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    connect(ui->lowerLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
    connect(ui->upperLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
    connect(ui->autoLimitsCB, &QCheckBox::clicked, this, &YAxisTool::updateAutoValues);
    connect(ui->defaultB, &QPushButton::clicked, this, &YAxisTool::updateToDefaults);
    connect(ui->colorB, &QPushButton::clicked, this, &YAxisTool::updateColor);
    connect(ui->leftAxisCB, &QCheckBox::clicked, this, &YAxisTool::updateLeftAxis);
}

void YAxisTool::updateValues(const double value)
{
    Q_UNUSED(value);
    YAxisInfo info = m_Info;
    info.axis->setRange(QCPRange(ui->lowerLimitSpin->value(), ui->upperLimitSpin->value()));
    info.rescale = ui->autoLimitsCB->isChecked();
    m_Info = info;
    updateSpins();
    emit axisChanged(m_Key, m_Info);
}

void YAxisTool::updateLeftAxis()
{
    ui->leftAxisCB->setEnabled(!ui->leftAxisCB->isChecked());
    emit leftAxisChanged(m_Info.axis);
}

void YAxisTool::computeAutoLimits()
{
    if (ui->autoLimitsCB->isChecked())
    {
        // The user checked the auto button.
        // Need to set the lower/upper spin boxes with the new auto limits.
        QCPAxis *axis = m_Info.axis;
        axis->rescale();
        axis->scaleRange(1.1, axis->range().center());

        disconnect(ui->lowerLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
        disconnect(ui->upperLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
        ui->lowerLimitSpin->setValue(axis->range().lower);
        ui->upperLimitSpin->setValue(axis->range().upper);
        connect(ui->lowerLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
        connect(ui->upperLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
    }
}

void YAxisTool::updateAutoValues()
{
    computeAutoLimits();
    updateValues(0);
}

void YAxisTool::updateToDefaults()
{
    m_Info.axis->setRange(m_Info.initialRange);
    m_Info.rescale = YAxisInfo::isRescale(m_Info.axis->range());
    m_Info.color = m_Info.initialColor;
    replot(ui->leftAxisCB->isChecked());
    computeAutoLimits();
    emit axisChanged(m_Key, m_Info);
}

void YAxisTool::replot(bool isLeftAxis)
{
    // I was using editingFinished, but that signaled on change of focus.
    // ValueChanged/valueChanged can output on the outputSpins call.
    disconnect(ui->lowerLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
    disconnect(ui->upperLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
    ui->reset(m_Info);
    connect(ui->lowerLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);
    connect(ui->upperLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &YAxisTool::updateValues);

    ui->leftAxisCB->setChecked(isLeftAxis);
    ui->leftAxisCB->setEnabled(!isLeftAxis);
    updateSpins();

    ui->colorLabel->setText("");
    ui->colorLabel->setStyleSheet(QString("QLabel { background-color : %1; }").arg(m_Info.color.name()));
}

void YAxisTool::reset(QObject *key, const YAxisInfo &info, bool isLeftAxis)
{
    m_Info = info;
    m_Key = key;
    replot(isLeftAxis);
}

void YAxisTool::updateColor()
{
    QColor color = QColorDialog::getColor(m_Info.color, this);
    if (color.isValid())
    {
        ui->colorLabel->setStyleSheet(QString("QLabel { background-color : %1; }").arg(color.name()));
        m_Info.color = color;
        emit axisColorChanged(m_Key, m_Info, color);
    }
    return;
}

void YAxisTool::updateSpins()
{
    ui->lowerLimitSpin->setEnabled(!m_Info.rescale);
    ui->upperLimitSpin->setEnabled(!m_Info.rescale);
    ui->lowerLimitLabel->setEnabled(!m_Info.rescale);
    ui->upperLimitLabel->setEnabled(!m_Info.rescale);
}

void YAxisToolUI::reset(const YAxisInfo &info)
{

    statLabel->setText(info.name);
    statShortLabel->setText(info.shortName);
    lowerLimitSpin->setValue(info.axis->range().lower);
    upperLimitSpin->setValue(info.axis->range().upper);
    autoLimitsCB->setChecked(info.rescale);
}

// If the user hit's the 'X', still want to remove the live preview.
void YAxisTool::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    slotClosed();
}

void YAxisTool::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
}


void YAxisTool::slotClosed()
{
}

void YAxisTool::slotSaveChanges()
{
}

