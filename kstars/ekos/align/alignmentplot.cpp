/*  Target plot for alignment.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>, 2025

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "alignmentplot.h"
#include "klocalizedstring.h"
#include "dms.h"

// Helper functions
namespace
{
int innerradius = 10;
int outerradius = innerradius + 10;
int linelength = 40;

static inline QPointer<QCPItemEllipse> createCircle(QCustomPlot *parent, QPen pen, QBrush brush, double radius)
{
    QPointer<QCPItemEllipse> circle = new QCPItemEllipse(parent);

    // position them to plot coordinates
    circle->topLeft->setType(QCPItemPosition::ptPlotCoords);
    circle->bottomRight->setType(QCPItemPosition::ptPlotCoords);

    // define the bounding boxes of the circles
    circle->topLeft->setCoords(-radius, radius);     // top left
    circle->bottomRight->setCoords(radius, -radius); // bottom right

    // Style
    circle->setPen(pen);
    circle->setBrush(brush);

    return circle;
}

static inline QPointer<QCPItemTracer> createTracerCircle(QCustomPlot *parent, QPen pen, QBrush brush, double radius)
{
    QPointer<QCPItemTracer> marker = new QCPItemTracer(parent);
    marker->position->setType(QCPItemPosition::ptPlotCoords);
    marker->position->setCoords(0, 0);
    marker->setStyle(QCPItemTracer::tsCircle);
    marker->setSize(2 * radius);
    marker->setPen(pen);
    marker->setBrush(brush);
    marker->setLayer("overlay");

    return marker;
}

static inline QPointer<QCPItemLine> createArrow(QCustomPlot *parent, QCPItemTracer *marker, QPen pen)
{
    QPointer<QCPItemLine> stick = new QCPItemLine(parent);
    // Anchor both ends to the marker position:
    stick->start->setParentAnchor(marker->position);
    stick->end->setParentAnchor(marker->position);

    // Use pixel coordinates relative to the anchor:
    stick->start->setType(QCPItemPosition::ptAbsolute);
    stick->end->setType(QCPItemPosition::ptAbsolute);

    // create arrow head
    stick->setHead(QCPLineEnding(QCPLineEnding::esSpikeArrow, 10, 10));

    stick->setPen(pen);
    stick->setLayer("overlay");

    return stick;
}

static inline QPointer<QCPItemText> createLabel(QCustomPlot *parent, QCPItemLine* spike, Qt::Alignment align,
        const QPointF &pxOffset)
{
    auto *t = new QCPItemText(parent);
    t->position->setParentAnchor(spike->end);              // follow spike tip
    t->position->setType(QCPItemPosition::ptAbsolute);     // pixel offset
    t->position->setCoords(pxOffset);                      // small gap from tip
    t->setPositionAlignment(align);

    t->setPen(Qt::NoPen);                                  // no frame
    t->setColor(Qt::white);                                // white text
    QFont f;
    f.setBold(true);                                       // bold
    // leave point size unset -> Qt uses default application font size
    t->setFont(f);
    t->setText("");                                        // start hidden
    t->setVisible(false);
    t->setLayer("overlay");

    return t;
};

static inline void showError(double deg, QCPItemLine *posSpike, QCPItemText * posLbl, QCPItemLine *negSpike,
                             QCPItemText * negLbl)
{
    const bool pos = (deg * 3600 >= 1.0);  // >=  1 asec
    const bool neg = (deg * 3600 <= -1.0); // <= -1 asec

    negSpike->setVisible(neg);
    posSpike->setVisible(pos);

    posLbl->setVisible(pos);
    negLbl->setVisible(neg);

    if (pos)
        posLbl->setText(dms(deg).toDMSString(true));
    else if (neg)
        negLbl->setText(dms(-deg).toDMSString(true));
};
}

AlignmentPlot::AlignmentPlot(QWidget *parent): QCustomPlot(parent)
{
    Q_UNUSED(parent);
    initGraph();

    addGraph();
    buildTarget();

    replot();
}

void AlignmentPlot::initGraph()
{
    double accuracyRadius = 2.5;

    setBackground(QBrush(Qt::black));
    setSelectionTolerance(10);

    xAxis->setBasePen(QPen(Qt::white, 1));
    yAxis->setBasePen(QPen(Qt::white, 1));

    xAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setTickPen(QPen(Qt::white, 1));

    xAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));

    xAxis->setTickLabelColor(Qt::white);
    yAxis->setTickLabelColor(Qt::white);

    xAxis->setLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);

    xAxis->setLabelFont(QFont(font().family(), 10));
    yAxis->setLabelFont(QFont(font().family(), 10));

    xAxis->setLabelPadding(2);
    yAxis->setLabelPadding(2);

    xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    xAxis->grid()->setZeroLinePen(QPen(QColor(220, 220, 180)));
    yAxis->grid()->setZeroLinePen(QPen(QColor(220, 220, 180)));

    xAxis->setLabel(i18n("Azimuth diff (deg)"));
    yAxis->setLabel(i18n("Altitude diff (deg)"));

    xAxis->setRange(-accuracyRadius, accuracyRadius);
    yAxis->setRange(-accuracyRadius, accuracyRadius);

    setInteractions(QCP::iRangeZoom);
    setInteraction(QCP::iRangeDrag, true);
}

void AlignmentPlot::buildTarget()
{
    createCircle(this, QPen(Qt::green, 1, Qt::DotLine), Qt::NoBrush, 0.1);
    createCircle(this, QPen(Qt::green, 1, Qt::DotLine), Qt::NoBrush, 0.2);
    createCircle(this, QPen(Qt::green, 1, Qt::DotLine), Qt::NoBrush, 0.3);
    createCircle(this, QPen(Qt::green, 1, Qt::DotLine), Qt::NoBrush, 0.4);
    createCircle(this, QPen(Qt::green, 1), Qt::NoBrush, 0.5);
    createCircle(this, QPen(Qt::yellow, 1, Qt::DotLine), Qt::NoBrush, 0.6);
    createCircle(this, QPen(Qt::yellow, 1, Qt::DotLine), Qt::NoBrush, 0.7);
    createCircle(this, QPen(Qt::yellow, 1, Qt::DotLine), Qt::NoBrush, 0.8);
    createCircle(this, QPen(Qt::yellow, 1, Qt::DotLine), Qt::NoBrush, 0.9);
    createCircle(this, QPen(Qt::yellow, 1), Qt::NoBrush, 1.0);
    createCircle(this, QPen(Qt::red, 1, Qt::DotLine), Qt::NoBrush, 1.2);
    createCircle(this, QPen(Qt::red, 1, Qt::DotLine), Qt::NoBrush, 1.4);
    createCircle(this, QPen(Qt::red, 1, Qt::DotLine), Qt::NoBrush, 1.6);
    createCircle(this, QPen(Qt::red, 1, Qt::DotLine), Qt::NoBrush, 1.8);
    createCircle(this, QPen(Qt::red, 1), Qt::NoBrush, 2.0);
}

void AlignmentPlot::displayAlignment(double deltaAlt, double deltaAz)
{
    // initialize the position error circle if required
    if (currentError.isNull())
    {
        currentError = new TargetTag(this);
    }
    currentError->displayAlignment(deltaAlt, deltaAz);

    // ensure that the range is at least 2 deg and larger than the max error
    double range = std::max(2.0, 1.4 * std::max(std::fabs(deltaAz), std::fabs(deltaAlt)));
    xAxis->setRange(-range, range);
    yAxis->setRange(-range, range);

    replot();
}

TargetTag::TargetTag(QCustomPlot *parent) : QObject(parent)
{

    // create the circles
    innerCircle = createTracerCircle(parent, QPen(Qt::red, 5), Qt::NoBrush, innerradius);
    outerCircle = createTracerCircle(parent, QPen(Qt::red, 2), Qt::NoBrush, outerradius);

    // create the spikes
    upperSpike = createArrow(parent, outerCircle, QPen(Qt::red, 2));
    lowerSpike = createArrow(parent, outerCircle, QPen(Qt::red, 2));
    leftSpike = createArrow(parent, outerCircle, QPen(Qt::red, 2));
    rightSpike = createArrow(parent, outerCircle, QPen(Qt::red, 2));

    constexpr double spikeGap = 4.0; // pixel gap outer circle

    upperSpike->start->setCoords(0, -outerradius - spikeGap);
    upperSpike->end->setCoords(0, -outerradius - linelength);
    lowerSpike->start->setCoords(0, outerradius + spikeGap);
    lowerSpike->end->setCoords(0, outerradius + linelength);

    leftSpike->start->setCoords(-outerradius - spikeGap, 0);
    leftSpike->end->setCoords(-outerradius - linelength, 0);
    rightSpike->start->setCoords(outerradius + spikeGap, 0);
    rightSpike->end->setCoords(outerradius + linelength, 0);

    constexpr double labelGap = 6.0; // pixel gap from arrow tip

    upperLabel = createLabel(parent, upperSpike, Qt::AlignHCenter | Qt::AlignBottom, QPointF(0, -labelGap));
    lowerLabel = createLabel(parent, lowerSpike, Qt::AlignHCenter | Qt::AlignTop,    QPointF(0,  labelGap));
    leftLabel  = createLabel(parent, leftSpike,  Qt::AlignHCenter | Qt::AlignTop,    QPointF(0,  labelGap));
    rightLabel = createLabel(parent, rightSpike, Qt::AlignHCenter | Qt::AlignTop,    QPointF(0,  labelGap));
}

void TargetTag::displayAlignment(double deltaAlt, double deltaAz)
{
    // move marker
    innerCircle->position->setCoords(deltaAz, deltaAlt);
    outerCircle->position->setCoords(deltaAz, deltaAlt);

    showError(deltaAlt, lowerSpike, lowerLabel, upperSpike, upperLabel);
    showError(deltaAz, leftSpike,  leftLabel, rightSpike, rightLabel);
}
