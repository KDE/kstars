/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skylabeler.h"

#include <cstdio>

#include <QPainter>
#include <QPixmap>

#include "Options.h"
#include "kstarsdata.h" // MINZOOM
#include "skymap.h"
#include "projections/projector.h"

//---------------------------------------------------------------------------//
// A Little data container class
//---------------------------------------------------------------------------//

typedef struct LabelRun
{
    LabelRun(int s, int e) : start(s), end(e) {}
    int start;
    int end;

} LabelRun;

//----- Now for the main event ----------------------------------------------//

//----- Static Methods ------------------------------------------------------//

SkyLabeler *SkyLabeler::pinstance = nullptr;

SkyLabeler *SkyLabeler::Instance()
{
    if (!pinstance)
        pinstance = new SkyLabeler();
    return pinstance;
}

void SkyLabeler::setZoomFont()
{
#ifndef KSTARS_LITE
    QFont font(m_p.font());
#else
    QFont font(m_stdFont);
#endif
    int deltaSize = 0;
    if (Options::zoomFactor() < 2.0 * MINZOOM)
        deltaSize = 2;
    else if (Options::zoomFactor() < 10.0 * MINZOOM)
        deltaSize = 1;

#ifndef KSTARS_LITE
    if (deltaSize)
    {
        font.setPointSize(font.pointSize() - deltaSize);
        m_p.setFont(font);
    }
#else
    if (deltaSize)
    {
        font.setPointSize(font.pointSize() - deltaSize);
    }
    if (m_drawFont.pointSize() != font.pointSize())
    {
        m_drawFont = font;
    }
#endif
}

double SkyLabeler::ZoomOffset()
{
    double offset = dms::PI * Options::zoomFactor() / 10800.0 / 3600.0;
    return 4.0 + offset * 0.5;
}

//----- Constructor ---------------------------------------------------------//

SkyLabeler::SkyLabeler()
    : m_fontMetrics(QFont()), m_picture(-1), labelList(NUM_LABEL_TYPES)
{
#ifdef KSTARS_LITE
    //Painter is needed to get default font and we use it only once to have only one warning
    m_stdFont = QFont();

//For some reason there is no point size in default font on Android
#ifdef ANDROID
    m_stdFont.setPointSize(16);
#else
    m_stdFont.setPointSize(m_stdFont.pointSize()+2);
#endif

#endif
}

SkyLabeler::~SkyLabeler()
{
    for (auto &row : screenRows)
    {
        for (auto &item : *row)
        {
            delete item;
        }
        delete row;
    }
}

bool SkyLabeler::drawGuideLabel(QPointF &o, const QString &text, double angle)
{
    // Create bounding rectangle by rotating the (height x width) rectangle
    qreal h = m_fontMetrics.height();
    qreal w = m_fontMetrics.width(text);
    qreal s = sin(angle * dms::PI / 180.0);
    qreal c = cos(angle * dms::PI / 180.0);

    qreal w2 = w / 2.0;

    qreal top, bot, left, right;

    // These numbers really do depend on the sign of the angle like this
    if (angle >= 0.0)
    {
        top   = o.y() - s * w2;
        bot   = o.y() + c * h + s * w2;
        left  = o.x() - c * w2 - s * h;
        right = o.x() + c * w2;
    }
    else
    {
        top   = o.y() + s * w2;
        bot   = o.y() + c * h - s * w2;
        left  = o.x() - c * w2;
        right = o.x() + c * w2 - s * h;
    }

    // return false if label would overlap existing label
    if (!markRegion(left, right, top, bot))
        return false;

    // for debugging the bounding rectangle:
    //psky.drawLine( QPointF( left,  top ), QPointF( right, top ) );
    //psky.drawLine( QPointF( right, top ), QPointF( right, bot ) );
    //psky.drawLine( QPointF( right, bot ), QPointF( left,  bot ) );
    //psky.drawLine( QPointF( left,  bot ), QPointF( left,  top ) );

    // otherwise draw the label and return true
    m_p.save();
    m_p.translate(o);

    m_p.rotate(angle); //rotate the coordinate system
    m_p.drawText(QPointF(-w2, h), text);
    m_p.restore(); //reset coordinate system

    return true;
}

bool SkyLabeler::drawNameLabel(SkyObject *obj, const QPointF &_p,
                               const qreal padding_factor)
{
    QString sLabel = obj->labelString();
    if (sLabel.isEmpty())
        return false;

    double offset = obj->labelOffset();
    QPointF p(_p.x() + offset, _p.y() + offset);

    if (!markText(p, sLabel, padding_factor))
    {
        return false;
    }
    else
    {
        double factor       = log(Options::zoomFactor() / 750.0);
        double newPointSize = qBound(12.0, factor * m_stdFont.pointSizeF(), 18.0);
        QFont zoomFont(m_p.font());
        zoomFont.setPointSizeF(newPointSize);
        m_p.setFont(zoomFont);
        m_p.drawText(p, sLabel);
        return true;
    }
}

void SkyLabeler::setFont(const QFont &font)
{
#ifndef KSTARS_LITE
    m_p.setFont(font);
#else
    m_drawFont = font;
#endif
    m_fontMetrics = QFontMetrics(font);
}

void SkyLabeler::setPen(const QPen &pen)
{
#ifdef KSTARS_LITE
    Q_UNUSED(pen);
#else
    m_p.setPen(pen);
#endif
}

void SkyLabeler::shrinkFont(int delta)
{
#ifndef KSTARS_LITE
    QFont font(m_p.font());
#else
    QFont font(m_drawFont);
#endif
    font.setPointSize(font.pointSize() - delta);
    setFont(font);
}

void SkyLabeler::useStdFont()
{
    setFont(m_stdFont);
}

void SkyLabeler::resetFont()
{
    setFont(m_skyFont);
}

void SkyLabeler::getMargins(const QString &text, float *left, float *right, float *top, float *bot)
{
    float height     = m_fontMetrics.height();
    float width      = m_fontMetrics.width(text);
    float sideMargin = m_fontMetrics.width("MM") + width / 2.0;

    // Create the margins within which it is okay to draw the label
    double winHeight;
    double winWidth;
#ifdef KSTARS_LITE
    winHeight = SkyMapLite::Instance()->height();
    winWidth  = SkyMapLite::Instance()->width();
#else
    winHeight = m_p.window().height();
    winWidth  = m_p.window().width();
#endif

    *right = winWidth - sideMargin;
    *left  = sideMargin;
    *top   = height;
    *bot   = winHeight - 2.0 * height;
}

void SkyLabeler::reset(SkyMap *skyMap)
{
    // ----- Set up Projector ---
    m_proj = skyMap->projector();
    // ----- Set up Painter -----
    if (m_p.isActive())
        m_p.end();
    m_picture = QPicture();
    m_p.begin(&m_picture);
    //This works around BUG 10496 in Qt
    m_p.drawPoint(0, 0);
    m_p.drawPoint(skyMap->width() + 1, skyMap->height() + 1);
    // ----- Set up Zoom Dependent Font -----

    m_stdFont = QFont(m_p.font());
    setZoomFont();
    m_skyFont     = m_p.font();
    m_fontMetrics = QFontMetrics(m_skyFont);
    m_minDeltaX   = (int)m_fontMetrics.width("MMMMM");

    // ----- Set up Zoom Dependent Offset -----
    m_offset = SkyLabeler::ZoomOffset();

    // ----- Prepare Virtual Screen -----
    m_yScale = (m_fontMetrics.height() + 1.0);

    int maxY = int(skyMap->height() / m_yScale);
    if (maxY < 1)
        maxY = 1; // prevents a crash below?

    int m_maxX = skyMap->width();
    m_size     = (maxY + 1) * m_maxX;

    // Resize if needed:
    if (maxY > m_maxY)
    {
        screenRows.resize(m_maxY);
        for (int y = m_maxY; y <= maxY; y++)
        {
            screenRows.append(new LabelRow());
        }
        //printf("resize: %d -> %d, size:%d\n", m_maxY, maxY, screenRows.size());
    }

    // Clear all pre-existing rows as needed

    int minMaxY = (maxY < m_maxY) ? maxY : m_maxY;

    for (int y = 0; y <= minMaxY; y++)
    {
        LabelRow *row = screenRows[y];

        for (auto &item : *row)
        {
            delete item;
        }
        row->clear();
    }

    // never decrease m_maxY:
    if (m_maxY < maxY)
        m_maxY = maxY;

    // reset the counters
    m_marks = m_hits = m_misses = m_elements = 0;

    //----- Clear out labelList -----
    for (auto &item : labelList)
    {
        item.clear();
    }
}

#ifdef KSTARS_LITE
void SkyLabeler::reset()
{
    SkyMapLite *skyMap = SkyMapLite::Instance();
    // ----- Set up Projector ---
    m_proj = skyMap->projector();

    //m_stdFont was moved to constructor
    setZoomFont();
    m_skyFont     = m_drawFont;
    m_fontMetrics = QFontMetrics(m_skyFont);
    m_minDeltaX   = (int)m_fontMetrics.width("MMMMM");
    // ----- Set up Zoom Dependent Offset -----
    m_offset = ZoomOffset();

    // ----- Prepare Virtual Screen -----
    m_yScale = (m_fontMetrics.height() + 1.0);

    int maxY = int(skyMap->height() / m_yScale);
    if (maxY < 1)
        maxY = 1; // prevents a crash below?

    int m_maxX = skyMap->width();
    m_size     = (maxY + 1) * m_maxX;

    // Resize if needed:
    if (maxY > m_maxY)
    {
        screenRows.resize(m_maxY);
        for (int y = m_maxY; y <= maxY; y++)
        {
            screenRows.append(new LabelRow());
        }
        //printf("resize: %d -> %d, size:%d\n", m_maxY, maxY, screenRows.size());
    }

    // Clear all pre-existing rows as needed

    int minMaxY = (maxY < m_maxY) ? maxY : m_maxY;

    for (int y = 0; y <= minMaxY; y++)
    {
        LabelRow *row = screenRows[y];
        for (int i = 0; i < row->size(); i++)
        {
            delete row->at(i);
        }
        row->clear();
    }

    // never decrease m_maxY:
    if (m_maxY < maxY)
        m_maxY = maxY;

    // reset the counters
    m_marks = m_hits = m_misses = m_elements = 0;

    //----- Clear out labelList -----
    for (int i = 0; i < labelList.size(); i++)
    {
        labelList[i].clear();
    }
}
#endif

void SkyLabeler::draw(QPainter &p)
{
    //FIXME: need a better soln. Apparently starting a painter
    //clears the picture.
    // But it's not like that's something that should be in the docs, right?
    // No, that's definitely better to leave to people to figure out on their own.
    if (m_p.isActive())
    {
        m_p.end();
    }
    m_picture.play(&p); //can't replay while it's being painted on
    //this is also undocumented btw.
    //m_p.begin(&m_picture);
}

// We use Run Length Encoding to hold the information instead of an array of
// chars.  This is both faster and smaller but the code is more complicated.
//
// This code is easy to break and hard to fix.

bool SkyLabeler::markText(const QPointF &p, const QString &text, qreal padding_factor)
{
    static const auto ramp_zoom = log10(MINZOOM) + log10(MAXZOOM) * .3;

    if (padding_factor != 1)
    {
        padding_factor =
            (1 - ((std::min(log10(Options::zoomFactor()), ramp_zoom)) / ramp_zoom)) *
                padding_factor +
            1;
    }

    const qreal maxX = p.x() + m_fontMetrics.width(text) * padding_factor;
    const qreal minY = p.y() - m_fontMetrics.height() * padding_factor;
    return markRegion(p.x(), maxX, p.y(), minY);
}

bool SkyLabeler::markRegion(qreal left, qreal right, qreal top, qreal bot)
{
    if (m_maxY < 1)
    {
        if (!m_errors++)
            qDebug() << QString("Someone forgot to reset the SkyLabeler!");
        return true;
    }

    // setup x coordinates of rectangular region
    int minX = int(left);
    int maxX = int(right);
    if (maxX < minX)
    {
        maxX = minX;
        minX = int(right);
    }

    // setup y coordinates
    int maxY = int(bot / m_yScale);
    int minY = int(top / m_yScale);

    if (maxY < 0)
        maxY = 0;
    if (maxY > m_maxY)
        maxY = m_maxY;
    if (minY < 0)
        minY = 0;
    if (minY > m_maxY)
        minY = m_maxY;

    if (maxY < minY)
    {
        int temp = maxY;
        maxY     = minY;
        minY     = temp;
    }

    // check to see if we overlap any existing label
    // We must check all rows before we start marking
    for (int y = minY; y <= maxY; y++)
    {
        LabelRow *row = screenRows[y];
        int i;
        for (i = 0; i < row->size(); i++)
        {
            if (row->at(i)->end < minX)
                continue; // skip past these
            if (row->at(i)->start > maxX)
                break;
            m_misses++;
            return false;
        }
    }

    m_hits++;
    m_marks += (maxX - minX + 1) * (maxY - minY + 1);

    // Okay, there was no overlap so let's insert the current rectangle into
    // screenRows.

    for (int y = minY; y <= maxY; y++)
    {
        LabelRow *row = screenRows[y];

        // Simplest case: an empty row
        if (row->size() < 1)
        {
            row->append(new LabelRun(minX, maxX));
            m_elements++;
            continue;
        }

        // Find out our place in the universe (or row).
        // H'mm.  Maybe we could cache these numbers above.
        int i;
        for (i = 0; i < row->size(); i++)
        {
            if (row->at(i)->end >= minX)
                break;
        }

        // i now points to first label PAST ours

        // if we are first, append or merge at start of list
        if (i == 0)
        {
            if (row->at(0)->start - maxX < m_minDeltaX)
            {
                row->at(0)->start = minX;
            }
            else
            {
                row->insert(0, new LabelRun(minX, maxX));
                m_elements++;
            }
            continue;
        }

        // if we are past the last label, merge or append at end
        else if (i == row->size())
        {
            if (minX - row->at(i - 1)->end < m_minDeltaX)
            {
                row->at(i - 1)->end = maxX;
            }
            else
            {
                row->append(new LabelRun(minX, maxX));
                m_elements++;
            }
            continue;
        }

        // if we got here, we must insert or merge the new label
        //  between [i-1] and [i]

        bool mergeHead = (minX - row->at(i - 1)->end < m_minDeltaX);
        bool mergeTail = (row->at(i)->start - maxX < m_minDeltaX);

        // double merge => combine all 3 into one
        if (mergeHead && mergeTail)
        {
            row->at(i - 1)->end = row->at(i)->end;
            delete row->at(i);
            row->removeAt(i);
            m_elements--;
        }

        // Merge label with [i-1]
        else if (mergeHead)
        {
            row->at(i - 1)->end = maxX;
        }

        // Merge label with [i]
        else if (mergeTail)
        {
            row->at(i)->start = minX;
        }

        // insert between the two
        else
        {
            row->insert(i, new LabelRun(minX, maxX));
            m_elements++;
        }
    }

    return true;
}

void SkyLabeler::addLabel(SkyObject *obj, SkyLabeler::label_t type)
{
    bool visible = false;
    QPointF p    = m_proj->toScreen(obj, true, &visible);
    if (!visible || !m_proj->onScreen(p) || obj->translatedName().isEmpty())
        return;
    labelList[(int)type].append(SkyLabel(p, obj));
}

#ifdef KSTARS_LITE
void SkyLabeler::addLabel(SkyObject *obj, QPointF pos, label_t type)
{
    labelList[(int)type].append(SkyLabel(pos, obj));
}
#endif

void SkyLabeler::drawQueuedLabels()
{
    KStarsData *data = KStarsData::Instance();

    resetFont();
    m_p.setPen(QColor(data->colorScheme()->colorNamed("PNameColor")));
    drawQueuedLabelsType(PLANET_LABEL);

    if (labelList[SATURN_MOON_LABEL].size() > 0)
    {
        shrinkFont(2);
        drawQueuedLabelsType(SATURN_MOON_LABEL);
        resetFont();
    }

    if (labelList[JUPITER_MOON_LABEL].size() > 0)
    {
        shrinkFont(2);
        drawQueuedLabelsType(JUPITER_MOON_LABEL);
        resetFont();
    }

    // No colors for these fellas? Just following planets along?
    drawQueuedLabelsType(ASTEROID_LABEL);
    drawQueuedLabelsType(COMET_LABEL);

    m_p.setPen(QColor(data->colorScheme()->colorNamed("SatLabelColor")));
    drawQueuedLabelsType(SATELLITE_LABEL);

    // Whelp we're here and we don't have a Rude Label color?
    // Will just set it to Planet color since this is how it used to be!!
    m_p.setPen(QColor(data->colorScheme()->colorNamed("PNameColor")));
    LabelList list = labelList[RUDE_LABEL];

    for (const auto &item : list)
    {
        drawRudeNameLabel(item.obj, item.o);
    }
}

void SkyLabeler::drawQueuedLabelsType(SkyLabeler::label_t type)
{
    LabelList list = labelList[type];

    for (const auto &item : list)
    {
        drawNameLabel(item.obj, item.o);
    }
}

//Rude name labels don't check for collisions with other labels,
//these get drawn no matter what.  Transient labels are rude labels.
//To mitigate confusion from possibly "underlapping" labels, paint a
//semi-transparent background.
void SkyLabeler::drawRudeNameLabel(SkyObject *obj, const QPointF &p)
{
    QString sLabel = obj->labelString();
    double offset  = obj->labelOffset();
    QRectF rect    = m_p.fontMetrics().boundingRect(sLabel);
    rect.moveTo(p.x() + offset, p.y() + offset);

    //Interestingly, the fontMetric boundingRect isn't where you might think...
    //We need to tweak rect to get the BG rectangle rect2
    QRectF rect2 = rect;
    rect2.moveTop(rect.top() - 0.6 * rect.height());
    rect2.setHeight(0.8 * rect.height());

    //FIXME: Implement label background options
    QColor color(KStarsData::Instance()->colorScheme()->colorNamed("SkyColor"));
    color.setAlpha(m_p.pen().color().alpha()); //same transparency for the text and the background
    m_p.fillRect(rect2, QBrush(color));
    m_p.drawText(rect.topLeft(), sLabel);
}

//----- Diagnostic and information routines -----

float SkyLabeler::fillRatio()
{
    if (m_size == 0)
        return 0.0;
    return 100.0 * float(m_marks) / float(m_size);
}

float SkyLabeler::hitRatio()
{
    if (m_hits == 0)
        return 0.0;
    return 100.0 * float(m_hits) / (float(m_hits + m_misses));
}

void SkyLabeler::printInfo()
{
    printf("SkyLabeler:\n");
    printf("  fillRatio=%.1f%%\n", fillRatio());
    printf("  hits=%d  misses=%d  ratio=%.1f%%\n", m_hits, m_misses, hitRatio());
    printf("  yScale=%.1f maxY=%d\n", m_yScale, m_maxY);

    printf("  screenRows=%d elements=%d virtualSize=%.1f Kbytes\n", screenRows.size(), m_elements,
           float(m_size) / 1024.0);

//    static const char *labelName[NUM_LABEL_TYPES];
//
//    labelName[STAR_LABEL]         = "Star";
//    labelName[ASTEROID_LABEL]     = "Asteroid";
//    labelName[COMET_LABEL]        = "Comet";
//    labelName[PLANET_LABEL]       = "Planet";
//    labelName[JUPITER_MOON_LABEL] = "Jupiter Moon";
//    labelName[SATURN_MOON_LABEL]  = "Saturn Moon";
//    labelName[DEEP_SKY_LABEL]     = "Deep Sky Object";
//    labelName[CONSTEL_NAME_LABEL] = "Constellation Name";
//
//    for (int i = 0; i < NUM_LABEL_TYPES; i++)
//    {
//        printf("  %20ss: %d\n", labelName[i], labelList[i].size());
//    }
//
//    // Check for errors in the data structure
//    for (int y = 0; y <= m_maxY; y++)
//    {
//        LabelRow *row = screenRows[y];
//        int size      = row->size();
//        if (size < 2)
//            continue;
//
//        bool error = false;
//        for (int i = 1; i < size; i++)
//        {
//            if (row->at(i - 1)->end > row->at(i)->start)
//                error = true;
//        }
//        if (!error)
//            continue;
//
//        printf("ERROR: %3d: ", y);
//        for (int i = 0; i < row->size(); i++)
//        {
//            printf("(%d, %d) ", row->at(i)->start, row->at(i)->end);
//        }
//        printf("\n");
//    }
}
