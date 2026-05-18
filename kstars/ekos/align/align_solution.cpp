/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "alignview.h"
#include "auxiliary/QProgressIndicator.h"
#include "ekos/manager.h"
#include "ksnotification.h"
#include "polaralignmentassistant.h"
#include "qtcompat.h"
#include "Options.h"

#include "auxiliary/ksmessagebox.h"
#include "fitsviewer/fitsviewer.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"

#include <ekos_align_debug.h>

#include <QFileDialog>
#include <QToolTip>

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

void Align::selectSolutionTableRow(int row, int column)
{
    Q_UNUSED(column)

    solutionTable->selectRow(row);
    for (int i = 0; i < alignPlot->itemCount(); i++)
    {
        QCPAbstractItem *abstractItem = alignPlot->item(i);
        if (abstractItem)
        {
            QCPItemText *item = qobject_cast<QCPItemText *>(abstractItem);
            if (item)
            {
                if (i == row)
                {
                    item->setColor(Qt::black);
                    item->setBrush(Qt::yellow);
                }
                else
                {
                    item->setColor(Qt::red);
                    item->setBrush(Qt::white);
                }
            }
        }
    }
    alignPlot->replot();
}

void Align::handleHorizontalPlotSizeChange()
{
    alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);
    alignPlot->replot();
}

void Align::handleVerticalPlotSizeChange()
{
    alignPlot->yAxis->setScaleRatio(alignPlot->xAxis, 1.0);
    alignPlot->replot();
}

void Align::resizeEvent(QResizeEvent *event)
{
    if (event->oldSize().width() != -1)
    {
        if (event->oldSize().width() != size().width())
            handleHorizontalPlotSizeChange();
        else if (event->oldSize().height() != size().height())
            handleVerticalPlotSizeChange();
    }
    else
    {
        QTimer::singleShot(10, this, &Ekos::Align::handleHorizontalPlotSizeChange);
    }
}

void Align::handlePointTooltip(QMouseEvent *event)
{
    QCPAbstractItem *item = alignPlot->itemAt(QtCompat::mousePos(event));
    if (item)
    {
        QCPItemText *label = qobject_cast<QCPItemText *>(item);
        if (label)
        {
            QString labelText = label->text();
            int point         = labelText.toInt() - 1;

            if (point < 0)
                return;
            QToolTip::showText(QtCompat::mouseGlobalPos(event).toPoint(),
                               i18n("<table>"
                 "<tr>"
                 "<th colspan=\"2\">Object %1: %2</th>"
                 "</tr>"
                 "<tr>"
                 "<td>RA:</td><td>%3</td>"
                 "</tr>"
                 "<tr>"
                 "<td>DE:</td><td>%4</td>"
                 "</tr>"
                 "<tr>"
                 "<td>dRA:</td><td>%5</td>"
                 "</tr>"
                 "<tr>"
                 "<td>dDE:</td><td>%6</td>"
                 "</tr>"
                 "</table>",
                                    point + 1,
                                    solutionTable->item(point, 2)->text(),
                                    solutionTable->item(point, 0)->text(),
                                    solutionTable->item(point, 1)->text(),
                                    solutionTable->item(point, 4)->text(),
                                    solutionTable->item(point, 5)->text()),
                               alignPlot, alignPlot->rect());
        }
    }
}

void Align::buildTarget()
{
    double accuracyRadius = alignAccuracyThreshold->value();
    if (centralTarget)
    {
        concentricRings->data()->clear();
        redTarget->data()->clear();
        yellowTarget->data()->clear();
        centralTarget->data()->clear();
    }
    else
    {
        concentricRings = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
        redTarget       = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
        yellowTarget    = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
        centralTarget   = new QCPCurve(alignPlot->xAxis, alignPlot->yAxis);
    }
    const int pointCount = 200;
    QVector<QCPCurveData> circleRings(
        pointCount * (5)); //Have to multiply by the number of rings, Rings at : 25%, 50%, 75%, 125%, 175%
    QVector<QCPCurveData> circleCentral(pointCount);
    QVector<QCPCurveData> circleYellow(pointCount);
    QVector<QCPCurveData> circleRed(pointCount);

    int circleRingPt = 0;
    for (int i = 0; i < pointCount; i++)
    {
        double theta = i / static_cast<double>(pointCount) * 2 * M_PI;

        for (double ring = 1; ring < 8; ring++)
        {
            if (ring != 4 && ring != 6)
            {
                if (i % (9 - static_cast<int>(ring)) == 0) //This causes fewer points to draw on the inner circles.
                {
                    circleRings[circleRingPt] = QCPCurveData(circleRingPt, accuracyRadius * ring * 0.25 * qCos(theta),
                                                accuracyRadius * ring * 0.25 * qSin(theta));
                    circleRingPt++;
                }
            }
        }

        circleCentral[i] = QCPCurveData(i, accuracyRadius * qCos(theta), accuracyRadius * qSin(theta));
        circleYellow[i]  = QCPCurveData(i, accuracyRadius * 1.5 * qCos(theta), accuracyRadius * 1.5 * qSin(theta));
        circleRed[i]     = QCPCurveData(i, accuracyRadius * 2 * qCos(theta), accuracyRadius * 2 * qSin(theta));
    }

    concentricRings->setLineStyle(QCPCurve::lsNone);
    concentricRings->setScatterSkip(0);
    concentricRings->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(255, 255, 255, 150), 1));

    concentricRings->data()->set(circleRings, true);
    redTarget->data()->set(circleRed, true);
    yellowTarget->data()->set(circleYellow, true);
    centralTarget->data()->set(circleCentral, true);

    concentricRings->setPen(QPen(Qt::white));
    redTarget->setPen(QPen(Qt::red));
    yellowTarget->setPen(QPen(Qt::yellow));
    centralTarget->setPen(QPen(Qt::green));

    concentricRings->setBrush(Qt::NoBrush);
    redTarget->setBrush(QBrush(QColor(255, 0, 0, 50)));
    yellowTarget->setBrush(
        QBrush(QColor(0, 255, 0, 50))); //Note this is actually yellow.  It is green on top of red with equal opacity.
    centralTarget->setBrush(QBrush(QColor(0, 255, 0, 50)));

    if (alignPlot->size().width() > 0)
        alignPlot->replot();
}

void Align::slotAutoScaleGraph()
{
    double accuracyRadius = alignAccuracyThreshold->value();
    alignPlot->xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    alignPlot->yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);

    alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);

    alignPlot->replot();
}

void Align::slotClearAllSolutionPoints()
{
    if (solutionTable->rowCount() == 0)
        return;

    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
        KSMessageBox::Instance()->disconnect(this);

        solutionTable->setRowCount(0);
        alignPlot->graph(0)->data()->clear();
        alignPlot->clearItems();
        buildTarget();

        slotAutoScaleGraph();

    });

    KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to clear all of the solution points?"),
                                            i18n("Clear Solution Points"), 60);
}

void Align::slotRemoveSolutionPoint()
{
    auto abstractItem = alignPlot->item(solutionTable->currentRow());
    if (abstractItem)
    {
        auto item = qobject_cast<QCPItemText *>(abstractItem);
        if (item)
        {
            double point = item->position->key();
            alignPlot->graph(0)->data()->remove(point);
        }
    }

    alignPlot->removeItem(solutionTable->currentRow());

    for (int i = 0; i < alignPlot->itemCount(); i++)
    {
        auto oneItem = alignPlot->item(i);
        if (oneItem)
        {
            auto item = qobject_cast<QCPItemText *>(oneItem);
            if (item)
                item->setText(QString::number(i + 1));
        }
    }
    solutionTable->removeRow(solutionTable->currentRow());
    alignPlot->replot();
}

QProgressIndicator * Align::getProgressStatus()
{
    int currentRow = solutionTable->rowCount() - 1;

    // check if the current row indicates a progress state
    // 1. no row present
    if (currentRow < 0)
        return nullptr;
    // 2. indicator is not present or not a progress indicator
    QWidget *indicator = solutionTable->cellWidget(currentRow, 3);
    if (indicator == nullptr)
        return nullptr;
    return dynamic_cast<QProgressIndicator *>(indicator);
}

void Align::stopProgressAnimation()
{
    QProgressIndicator *progress_indicator = getProgressStatus();
    if (progress_indicator != nullptr)
        progress_indicator->stopAnimation();
}

void Align::setAlignTableResult(AlignResult result)
{
    // Do nothing if the progress indicator is not running.
    // This is necessary since it could happen that a problem occurs
    // before #captureAndSolve() has been started and there does not
    // exist a table entry for the current run.
    QProgressIndicator *progress_indicator = getProgressStatus();
    if (progress_indicator == nullptr || ! progress_indicator->isAnimated())
        return;
    stopProgressAnimation();

    QIcon icon;
    switch (result)
    {
        case ALIGN_RESULT_SUCCESS:
            icon = QIcon(":/icons/AlignSuccess.svg");
            break;

        case ALIGN_RESULT_WARNING:
            icon = QIcon(":/icons/AlignWarning.svg");
            break;

        case ALIGN_RESULT_FAILED:
        default:
            icon = QIcon(":/icons/AlignFailure.svg");
            break;
    }
    int currentRow = solutionTable->rowCount() - 1;
    solutionTable->setCellWidget(currentRow, 3, new QWidget());
    QTableWidgetItem *statusReport = new QTableWidgetItem();
    statusReport->setIcon(icon);
    statusReport->setFlags(Qt::ItemIsSelectable);
    solutionTable->setItem(currentRow, 3, statusReport);
}

void Align::calculateAlignTargetDiff()
{
    // Normal align: Target coords are destinations coords
    // JM 2025.09.17: Calculate anyway, we don't need to take any actions.
    m_TargetDiffRA = (m_AlignCoord.ra().deltaAngle(m_TargetCoord.ra())).Degrees() * 3600;  // arcsec
    m_TargetDiffDE = (m_AlignCoord.dec().deltaAngle(m_TargetCoord.dec())).Degrees() * 3600;  // arcsec

    // Must update target coordinate horizontal coordinates.
    m_TargetCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    m_TargetDiffAZ = (m_AlignCoord.az().deltaAngle(m_TargetCoord.az())).Degrees() * 3600;  // arcsec
    m_TargetDiffAL = (m_AlignCoord.alt().deltaAngle(m_TargetCoord.alt())).Degrees() * 3600;  // arcsec

    if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) ||
            matchPAHStage(PAA::PAH_SECOND_CAPTURE) ||
            matchPAHStage(PAA::PAH_THIRD_CAPTURE) ||
            matchPAHStage(PAA::PAH_FIRST_SOLVE) ||
            matchPAHStage(PAA::PAH_SECOND_SOLVE) ||
            matchPAHStage(PAA::PAH_THIRD_SOLVE) ||
            syncR->isChecked())
        return;

    // Differential slewing: Target coords are new position coords
    if (Options::astrometryDifferentialSlewing())
    {
        m_TargetDiffRA = (m_AlignCoord.ra().deltaAngle(m_DestinationCoord.ra())).Degrees() * 3600;  // arcsec
        m_TargetDiffDE = (m_AlignCoord.dec().deltaAngle(m_DestinationCoord.dec())).Degrees() * 3600;  // arcsec
        m_TargetDiffAZ = (m_AlignCoord.az().deltaAngle(m_DestinationCoord.az())).Degrees() * 3600;  // arcsec
        m_TargetDiffAL = (m_AlignCoord.alt().deltaAngle(m_DestinationCoord.alt())).Degrees() * 3600;  // arcsec
        qCDebug(KSTARS_EKOS_ALIGN) << "Differential slew - Solution RA:" << m_AlignCoord.ra().toHMSString()
                                   << " DE:" << m_AlignCoord.dec().toDMSString();
        qCDebug(KSTARS_EKOS_ALIGN) << "Differential slew - Destination RA:" << m_DestinationCoord.ra().toHMSString()
                                   << " DE:" << m_DestinationCoord.dec().toDMSString();
    }

    m_TargetDiffTotal = sqrt(m_TargetDiffRA * m_TargetDiffRA + m_TargetDiffDE * m_TargetDiffDE);

    errOut->setText(QString("%1 arcsec. RA:%2 DE:%3").arg(
                        QString::number(m_TargetDiffTotal, 'f', 0),
                        QString::number(m_TargetDiffRA, 'f', 0),
                        QString::number(m_TargetDiffDE, 'f', 0)));
    if (m_TargetDiffTotal <= static_cast<double>(alignAccuracyThreshold->value()))
        errOut->setStyleSheet("color:green");
    else if (m_TargetDiffTotal < 1.5 * alignAccuracyThreshold->value())
        errOut->setStyleSheet("color:yellow");
    else
        errOut->setStyleSheet("color:red");

    //This block of code will write the result into the solution table and plot it on the graph.
    int currentRow = solutionTable->rowCount() - 1;
    QTableWidgetItem *dRAReport = new QTableWidgetItem();
    if (dRAReport)
    {
        dRAReport->setText(QString::number(m_TargetDiffRA, 'f', 3) + "\"");
        dRAReport->setTextAlignment(Qt::AlignHCenter);
        dRAReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 4, dRAReport);
    }

    QTableWidgetItem *dDECReport = new QTableWidgetItem();
    if (dDECReport)
    {
        dDECReport->setText(QString::number(m_TargetDiffDE, 'f', 3) + "\"");
        dDECReport->setTextAlignment(Qt::AlignHCenter);
        dDECReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 5, dDECReport);
    }

    double raPlot  = m_TargetDiffRA;
    double decPlot = m_TargetDiffDE;
    alignPlot->graph(0)->addData(raPlot, decPlot);

    QCPItemText *textLabel = new QCPItemText(alignPlot);
    textLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    textLabel->position->setType(QCPItemPosition::ptPlotCoords);
    textLabel->position->setCoords(raPlot, decPlot);
    textLabel->setColor(Qt::red);
    textLabel->setPadding(QMargins(0, 0, 0, 0));
    textLabel->setBrush(Qt::white);
    textLabel->setPen(Qt::NoPen);
    textLabel->setText(' ' + QString::number(solutionTable->rowCount()) + ' ');
    textLabel->setFont(QFont(font().family(), 8));

    if (!alignPlot->xAxis->range().contains(m_TargetDiffRA))
    {
        alignPlot->graph(0)->rescaleKeyAxis(true);
        alignPlot->yAxis->setScaleRatio(alignPlot->xAxis, 1.0);
    }
    if (!alignPlot->yAxis->range().contains(m_TargetDiffDE))
    {
        alignPlot->graph(0)->rescaleValueAxis(true);
        alignPlot->xAxis->setScaleRatio(alignPlot->yAxis, 1.0);
    }
    alignPlot->replot();
}

void Align::exportSolutionPoints()
{
    if (solutionTable->rowCount() == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(Ekos::Manager::Instance(), i18nc("@title:window", "Export Solution Points"),
                      alignURLPath,
                      "CSV File (*.csv)");
    if (exportFile.isEmpty()) // if user presses cancel
        return;
    if (exportFile.toLocalFile().endsWith(QLatin1String(".csv")) == false)
        exportFile.setPath(exportFile.toLocalFile() + ".csv");

    QString path = exportFile.toLocalFile();

    if (QFile::exists(path))
    {
        int r = KMessageBox::warningContinueCancel(nullptr,
                i18n("A file named \"%1\" already exists. "
             "Overwrite it?",
                     exportFile.fileName()),
                i18n("Overwrite File?"), KStandardGuiItem::overwrite());
        if (r == KMessageBox::Cancel)
            return;
    }

    if (!exportFile.isValid())
    {
        QString message = i18n("Invalid URL: %1", exportFile.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
        return;
    }

    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", path);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return;
    }

    QTextStream outstream(&file);

    QString epoch = QString::number(KStarsDateTime::currentDateTime().epoch());

    outstream << "RA (J" << epoch << "),DE (J" << epoch
              << "),RA (degrees),DE (degrees),Name,RA Error (arcsec),DE Error (arcsec)" << Qt::endl;

    for (int i = 0; i < solutionTable->rowCount(); i++)
    {
        QTableWidgetItem *raCell      = solutionTable->item(i, 0);
        QTableWidgetItem *deCell      = solutionTable->item(i, 1);
        QTableWidgetItem *objNameCell = solutionTable->item(i, 2);
        QTableWidgetItem *raErrorCell = solutionTable->item(i, 4);
        QTableWidgetItem *deErrorCell = solutionTable->item(i, 5);

        if (!raCell || !deCell || !objNameCell || !raErrorCell || !deErrorCell)
        {
            KSNotification::sorry(i18n("Error in table structure."));
            return;
        }
        dms raDMS = dms::fromString(raCell->text(), false);
        dms deDMS = dms::fromString(deCell->text(), true);
        outstream << raDMS.toHMSString() << ',' << deDMS.toDMSString() << ',' << raDMS.Degrees() << ','
                  << deDMS.Degrees() << ',' << objNameCell->text() << ',' << raErrorCell->text().remove('\"') << ','
                  << deErrorCell->text().remove('\"') << Qt::endl;
    }
    Q_EMIT newLog(i18n("Solution Points Saved as: %1", path));
    file.close();
}

void Align::zoomAlignView()
{
    m_AlignView->ZoomDefault();

    // Frame update is not immediate to reduce too many refreshes
    // So emit updated frame in 500ms
    QTimer::singleShot(500, this, [this]()
    {
        Q_EMIT newFrame(m_AlignView);
    });
}

void Align::setAlignZoom(double scale)
{
    if (scale > 1)
        m_AlignView->ZoomIn();
    else if (scale < 1)
        m_AlignView->ZoomOut();

    // Frame update is not immediate to reduce too many refreshes
    // So emit updated frame in 500ms
    QTimer::singleShot(500, this, [this]()
    {
        Q_EMIT newFrame(m_AlignView);
    });
}

void Align::showFITSViewer()
{
    static int lastFVTabID = -1;
    if (m_ImageData)
    {
        QUrl url = QUrl::fromLocalFile("align.fits");
        if (fv.isNull())
        {
            fv = KStars::Instance()->createFITSViewer();
            fv->loadData(m_ImageData, url, &lastFVTabID);
            connect(fv.get(), &FITSViewer::terminated, this, [this]()
            {
                fv.clear();
            });
        }
        else if (fv->updateData(m_ImageData, url, lastFVTabID, &lastFVTabID) == false)
            fv->loadData(m_ImageData, url, &lastFVTabID);

        fv->show();
    }
}

void Align::toggleAlignWidgetFullScreen()
{
    if (alignWidget->parent() == nullptr)
    {
        alignWidget->setParent(this);
        rightLayout->insertWidget(0, alignWidget);
        alignWidget->showNormal();
    }
    else
    {
        alignWidget->setParent(nullptr);
        alignWidget->setWindowTitle(i18nc("@title:window", "Align Frame"));
        alignWidget->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        alignWidget->showMaximized();
        alignWidget->show();
    }
}

}
