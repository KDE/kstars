/*
    SPDX-FileCopyrightText: Kitware Inc.
    SPDX-License-Identifier: Apache-2.0

    Modified from the original code to support 3 sliders, May 2023.
*/

// Qt includes
#include <QDebug>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyleOptionSlider>
#include <QApplication>
#include <QStylePainter>
#include <QStyle>
#include <QToolTip>

#include "ctk3slider.h"

class ctk3SliderPrivate
{
        Q_DECLARE_PUBLIC(ctk3Slider)
    protected:
        ctk3Slider* const q_ptr;
    public:
        /// Boolean indicates the selected handle
        ///   True for the minimum range handle, false for the maximum range handle
        enum Handle
        {
            NoHandle = 0x0,
            MinimumHandle = 0x1,
            MidHandle = 0x2,
            MaximumHandle = 0x4
        };
        Q_DECLARE_FLAGS(Handles, Handle)

        ctk3SliderPrivate(ctk3Slider &object);
        void init();

        /// Return the handle at the given pos, or none if no handle is at the pos.
        /// If a handle is selected, handleRect is set to the handle rect.
        /// otherwise return NoHandle and handleRect is set to the combined rect of
        /// the min and max handles
        Handle handleAtPos(const QPoint &pos, QRect &handleRect)const;

        /// Copied verbatim from QSliderPrivate class (see QSlider.cpp)
        int pixelPosToRangeValue(int pos) const;
        int pixelPosFromRangeValue(int val) const;

        /// Draw the bottom and top sliders.
        void drawMinimumSlider( QStylePainter* painter ) const;
        void drawMaximumSlider( QStylePainter* painter ) const;
        void drawMidSlider( QStylePainter* painter ) const;

        /// End points of the range on the Model
        int m_MaximumValue;
        int m_MinimumValue;
        int m_MidValue;

        /// End points of the range on the GUI. This is synced with the model.
        int m_MaximumPosition;
        int m_MinimumPosition;
        int m_MidPosition;

        /// Controls selected ?
        QStyle::SubControl m_MinimumSliderSelected;
        QStyle::SubControl m_MaximumSliderSelected;
        QStyle::SubControl m_MidSliderSelected;

        /// See QSliderPrivate::clickOffset.
        /// Overrides this ivar
        int m_SubclassClickOffset;

        /// See QSliderPrivate::position
        /// Overrides this ivar.
        int m_SubclassPosition;

        /// Original width between the 2 bounds before any moves
        int m_SubclassWidth;

        ctk3SliderPrivate::Handles m_SelectedHandles;

        QString m_HandleToolTip;

    private:
        Q_DISABLE_COPY(ctk3SliderPrivate)
};

// --------------------------------------------------------------------------
ctk3SliderPrivate::ctk3SliderPrivate(ctk3Slider &object)
    : q_ptr(&object)
{
    this->m_MinimumValue = 0;
    this->m_MaximumValue = 100;
    this->m_MidValue = 50;
    this->m_MinimumPosition = 0;
    this->m_MaximumPosition = 100;
    this->m_MidPosition = 50;
    this->m_MinimumSliderSelected = QStyle::SC_None;
    this->m_MaximumSliderSelected = QStyle::SC_None;
    this->m_MidSliderSelected = QStyle::SC_None;
    this->m_SubclassClickOffset = 0;
    this->m_SubclassPosition = 0;
    this->m_SubclassWidth = 0;
    this->m_SelectedHandles = ctk3SliderPrivate::NoHandle;
}

// --------------------------------------------------------------------------
void ctk3SliderPrivate::init()
{
    Q_Q(ctk3Slider);
    this->m_MinimumValue = q->minimum();
    this->m_MaximumValue = q->maximum();
    this->m_MaximumValue = (q->minimum() + q->maximum()) / 2.0;
    this->m_MinimumPosition = q->minimum();
    this->m_MaximumPosition = q->maximum();
    this->m_MidPosition = (q->minimum() + q->maximum()) / 2.0;
    q->connect(q, SIGNAL(rangeChanged(int, int)), q, SLOT(onRangeChanged(int, int)));
}

// --------------------------------------------------------------------------
ctk3SliderPrivate::Handle ctk3SliderPrivate::handleAtPos(const QPoint &pos, QRect &handleRect)const
{
    Q_Q(const ctk3Slider);

    QStyleOptionSlider option;
    q->initStyleOption( &option );

    // The functions hitTestComplexControl only know about 1 handle. As we have
    // 3, we change the position of the handle and test if the pos correspond to
    // any of the 3 positions.

    // Test the MinimumHandle
    option.sliderPosition = this->m_MinimumPosition;
    option.sliderValue    = this->m_MinimumValue;

    QStyle::SubControl minimumControl = q->style()->hitTestComplexControl(
                                            QStyle::CC_Slider, &option, pos, q);
    QRect minimumHandleRect = q->style()->subControlRect(
                                  QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, q);

    // Test if the pos is under the Maximum handle
    option.sliderPosition = this->m_MaximumPosition;
    option.sliderValue    = this->m_MaximumValue;

    QStyle::SubControl maximumControl = q->style()->hitTestComplexControl(
                                            QStyle::CC_Slider, &option, pos, q);
    QRect maximumHandleRect = q->style()->subControlRect(
                                  QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, q);

    // Test if the pos is under the Mid handle
    option.sliderPosition = this->m_MidPosition;
    option.sliderValue    = this->m_MidValue;

    QStyle::SubControl midControl = q->style()->hitTestComplexControl(
                                        QStyle::CC_Slider, &option, pos, q);
    QRect midHandleRect = q->style()->subControlRect(
                              QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, q);

    if (minimumControl == QStyle::SC_SliderHandle)
    {
        handleRect = minimumHandleRect;
        return MinimumHandle;
    }
    else if (midControl == QStyle::SC_SliderHandle)
    {
        handleRect = midHandleRect;
        return MidHandle;
    }
    else if (maximumControl == QStyle::SC_SliderHandle)
    {
        handleRect = maximumHandleRect;
        return MaximumHandle;
    }
    handleRect = minimumHandleRect.united(maximumHandleRect);
    return NoHandle;
}

// --------------------------------------------------------------------------
// Copied verbatim from QSliderPrivate::pixelPosToRangeValue. See QSlider.cpp
//
int ctk3SliderPrivate::pixelPosToRangeValue( int pos ) const
{
    Q_Q(const ctk3Slider);
    QStyleOptionSlider option;
    q->initStyleOption( &option );

    QRect gr = q->style()->subControlRect( QStyle::CC_Slider,
                                           &option,
                                           QStyle::SC_SliderGroove,
                                           q );
    QRect sr = q->style()->subControlRect( QStyle::CC_Slider,
                                           &option,
                                           QStyle::SC_SliderHandle,
                                           q );
    int sliderMin, sliderMax, sliderLength;
    if (option.orientation == Qt::Horizontal)
    {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
    }
    else
    {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
    }

    return QStyle::sliderValueFromPosition( q->minimum(),
                                            q->maximum(),
                                            pos - sliderMin,
                                            sliderMax - sliderMin,
                                            option.upsideDown );
}

//---------------------------------------------------------------------------
int ctk3SliderPrivate::pixelPosFromRangeValue( int val ) const
{
    Q_Q(const ctk3Slider);
    QStyleOptionSlider option;
    q->initStyleOption( &option );

    QRect gr = q->style()->subControlRect( QStyle::CC_Slider,
                                           &option,
                                           QStyle::SC_SliderGroove,
                                           q );
    QRect sr = q->style()->subControlRect( QStyle::CC_Slider,
                                           &option,
                                           QStyle::SC_SliderHandle,
                                           q );
    int sliderMin, sliderMax, sliderLength;
    if (option.orientation == Qt::Horizontal)
    {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
    }
    else
    {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
    }

    return QStyle::sliderPositionFromValue( q->minimum(),
                                            q->maximum(),
                                            val,
                                            sliderMax - sliderMin,
                                            option.upsideDown ) + sliderMin;
}

//---------------------------------------------------------------------------
// Draw slider at the bottom end of the range
void ctk3SliderPrivate::drawMinimumSlider( QStylePainter* painter ) const
{
    if (m_MinimumPosition > q_ptr->maximum() || m_MinimumPosition < q_ptr->minimum() )
        return;

    Q_Q(const ctk3Slider);
    QStyleOptionSlider option;
    q->initMinimumSliderStyleOption( &option );

    option.subControls = QStyle::SC_SliderHandle;
    option.sliderValue = m_MinimumValue;
    option.sliderPosition = m_MinimumPosition;
    if (q->isMinimumSliderDown())
    {
        option.activeSubControls = QStyle::SC_SliderHandle;
        option.state |= QStyle::State_Sunken;
    }
#ifdef Q_OS_MAC
    // On mac style, drawing just the handle actually draws also the groove.
    QRect clip = q->style()->subControlRect(QStyle::CC_Slider, &option,
                                            QStyle::SC_SliderHandle, q);
    painter->setClipRect(clip);
#endif
    painter->drawComplexControl(QStyle::CC_Slider, option);
}

//---------------------------------------------------------------------------
// Draw slider at the top end of the range
void ctk3SliderPrivate::drawMaximumSlider( QStylePainter* painter ) const
{
    if (m_MaximumPosition > q_ptr->maximum() || m_MaximumPosition < q_ptr->minimum() )
        return;

    Q_Q(const ctk3Slider);
    QStyleOptionSlider option;
    q->initMaximumSliderStyleOption( &option );

    option.subControls = QStyle::SC_SliderHandle;
    option.sliderValue = m_MaximumValue;
    option.sliderPosition = m_MaximumPosition;
    if (q->isMaximumSliderDown())
    {
        option.activeSubControls = QStyle::SC_SliderHandle;
        option.state |= QStyle::State_Sunken;
    }
#ifdef Q_OS_MAC
    // On mac style, drawing just the handle actually draws also the groove.
    QRect clip = q->style()->subControlRect(QStyle::CC_Slider, &option,
                                            QStyle::SC_SliderHandle, q);
    painter->setClipRect(clip);
#endif
    painter->drawComplexControl(QStyle::CC_Slider, option);
}

// Draw the mid slider
void ctk3SliderPrivate::drawMidSlider( QStylePainter* painter ) const
{
    if (m_MidPosition > q_ptr->maximum() || m_MidPosition < q_ptr->minimum() )
        return;

    Q_Q(const ctk3Slider);
    QStyleOptionSlider option;
    q->initMidSliderStyleOption( &option );

    option.subControls = QStyle::SC_SliderHandle;
    option.sliderValue = m_MidValue;
    option.sliderPosition = m_MidPosition;
    if (q->isMidSliderDown())
    {
        option.activeSubControls = QStyle::SC_SliderHandle;
        option.state |= QStyle::State_Sunken;
    }
#ifdef Q_OS_MAC
    // On mac style, drawing just the handle actually draws also the groove.
    QRect clip = q->style()->subControlRect(QStyle::CC_Slider, &option,
                                            QStyle::SC_SliderHandle, q);
    painter->setClipRect(clip);
#endif
    painter->drawComplexControl(QStyle::CC_Slider, option);
}

// --------------------------------------------------------------------------
ctk3Slider::ctk3Slider(QWidget* _parent)
    : QSlider(_parent)
    , d_ptr(new ctk3SliderPrivate(*this))
{
    Q_D(ctk3Slider);
    d->init();
}

// --------------------------------------------------------------------------
ctk3Slider::ctk3Slider( Qt::Orientation o,
                        QWidget* parentObject )
    : QSlider(o, parentObject)
    , d_ptr(new ctk3SliderPrivate(*this))
{
    Q_D(ctk3Slider);
    d->init();
}

// --------------------------------------------------------------------------
ctk3Slider::ctk3Slider(ctk3SliderPrivate* impl, QWidget* _parent)
    : QSlider(_parent)
    , d_ptr(impl)
{
    Q_D(ctk3Slider);
    d->init();
}

// --------------------------------------------------------------------------
ctk3Slider::ctk3Slider( ctk3SliderPrivate* impl, Qt::Orientation o,
                        QWidget* parentObject )
    : QSlider(o, parentObject)
    , d_ptr(impl)
{
    Q_D(ctk3Slider);
    d->init();
}

// --------------------------------------------------------------------------
ctk3Slider::~ctk3Slider()
{
}

// --------------------------------------------------------------------------
int ctk3Slider::minimumValue() const
{
    Q_D(const ctk3Slider);
    return d->m_MinimumValue;
}

// --------------------------------------------------------------------------
void ctk3Slider::setMinimumValue( int min )
{
    Q_D(ctk3Slider);
    // Min cannot be set higher than mid or max
    int newMin = qMin( qMin(min, d->m_MidValue), d->m_MaximumValue);
    this->setValues( newMin, d->m_MidValue, d->m_MaximumValue);
}

// --------------------------------------------------------------------------
int ctk3Slider::maximumValue() const
{
    Q_D(const ctk3Slider);
    return d->m_MaximumValue;
}

// --------------------------------------------------------------------------
void ctk3Slider::setMaximumValue( int max )
{
    Q_D(ctk3Slider);
    // Max cannot be set lower than min or mid
    int newMax = qMax( qMax(max, d->m_MidValue), d->m_MinimumValue);
    this->setValues(d->m_MinimumValue, d->m_MidValue, newMax);
}

// --------------------------------------------------------------------------
int ctk3Slider::midValue() const
{
    Q_D(const ctk3Slider);
    return d->m_MidValue;
}

// --------------------------------------------------------------------------
void ctk3Slider::setMidValue( int mid )
{
    Q_D(ctk3Slider);
    // Mid cannot be set lower than min or higher than max;
    int newMid = qMax(d->m_MinimumValue, mid);
    newMid = qMin(newMid, d->m_MaximumValue);
    this->setValues(d->m_MinimumValue, newMid, d->m_MaximumValue);
}

// --------------------------------------------------------------------------
namespace
{
// Sorts the 3 numbers so l becomes the least, and u becomes the largest.
void sortLMU(int &l, int &m, int &u)
{
    int kL = l, kM = m, kU = u;
    if (l <= m && l <= u) // l is the smallest
    {
        if (m <= u) // order is l,m,u
            return;
        else // order is l, u, m
        {
            l = kL;
            m = kU;
            u = kM;
        }
    }
    else if (m <= l && m <= u) // m is the smallest
    {
        if (l <= u) // order is m, l, u
        {
            l = kM;
            m = kL;
            u = kU;
        }
        else  // order is m, u, l
        {
            l = kM;
            m = kU;
            u = kL;
        }
    }
    else // u is the smallest
    {
        if (l <= m)  // order is u, l, m
        {
            l = kL;
            m = kU;
            u = kM;
            l = m;
        }
        else  // order is u, m, l
        {
            l = kU;
            m = kM;
            u = kL;
        }
    }
}
}
void ctk3Slider::setValues(int l, int m, int u)
{
    Q_D(ctk3Slider);
    sortLMU(l, m, u);

    // Can't bound with this->minimum() or maximum() when the system is zoomed in.
    const int minValue = l;
    const int maxValue = u;
    const int midValue = m;

    bool emitMinValChanged = (minValue != d->m_MinimumValue);
    bool emitMaxValChanged = (maxValue != d->m_MaximumValue);
    bool emitMidValChanged = (midValue != d->m_MidValue);

    d->m_MinimumValue = minValue;
    d->m_MaximumValue = maxValue;
    d->m_MidValue = midValue;

    bool emitMinPosChanged =
        (minValue != d->m_MinimumPosition);
    bool emitMaxPosChanged =
        (maxValue != d->m_MaximumPosition);
    bool emitMidPosChanged =
        (midValue != d->m_MidPosition);
    d->m_MinimumPosition = minValue;
    d->m_MaximumPosition = maxValue;
    d->m_MidPosition = midValue;

    if (isSliderDown())
    {
        if (emitMinPosChanged || emitMaxPosChanged || emitMidPosChanged)
        {
            emit positionsChanged(d->m_MinimumPosition, d->m_MidPosition, d->m_MaximumPosition);
        }
        if (emitMinPosChanged)
        {
            emit minimumPositionChanged(d->m_MinimumPosition);
        }
        if (emitMaxPosChanged)
        {
            emit maximumPositionChanged(d->m_MaximumPosition);
        }
        if (emitMidPosChanged)
        {
            emit midPositionChanged(d->m_MidPosition);
        }
    }
    if (emitMinValChanged || emitMaxValChanged || emitMidValChanged)
    {
        emit valuesChanged(d->m_MinimumValue, d->m_MidValue, d->m_MaximumValue);
    }
    if (emitMinValChanged)
    {
        emit minimumValueChanged(d->m_MinimumValue);
    }
    if (emitMaxValChanged)
    {
        emit maximumValueChanged(d->m_MaximumValue);
    }
    if (emitMidValChanged)
    {
        emit midValueChanged(d->m_MidValue);
    }
    if (emitMinPosChanged || emitMaxPosChanged || emitMidPosChanged ||
            emitMinValChanged || emitMaxValChanged || emitMidValChanged)
    {
        this->update();
    }
}

// --------------------------------------------------------------------------
int ctk3Slider::minimumPosition() const
{
    Q_D(const ctk3Slider);
    return d->m_MinimumPosition;
}

// --------------------------------------------------------------------------
int ctk3Slider::maximumPosition() const
{
    Q_D(const ctk3Slider);
    return d->m_MaximumPosition;
}
// --------------------------------------------------------------------------
int ctk3Slider::midPosition() const
{
    Q_D(const ctk3Slider);
    return d->m_MidPosition;
}

// --------------------------------------------------------------------------
void ctk3Slider::setMinimumPosition(int l)
{
    Q_D(const ctk3Slider);
    // Min cannot be set higher than mid or max
    int newMin = qMin( qMin(l, d->m_MidPosition), d->m_MaximumPosition);
    this->setPositions(newMin, d->m_MidPosition, d->m_MaximumPosition);
}

// --------------------------------------------------------------------------
void ctk3Slider::setMaximumPosition(int u)
{
    Q_D(const ctk3Slider);
    // Max cannot be set lower than min or mid
    int newMax = qMax( qMax(u, d->m_MidPosition), d->m_MinimumPosition);
    this->setPositions(d->m_MinimumPosition, d->m_MidPosition, newMax);
}
// --------------------------------------------------------------------------
void ctk3Slider::setMidPosition(int m)
{
    Q_D(const ctk3Slider);
    // Mid cannot be set lower than min or higher than max;
    int newMid = qMax(d->m_MinimumPosition, m);
    newMid = qMin(newMid, d->m_MaximumPosition);
    this->setPositions(d->m_MinimumPosition, newMid, d->m_MaximumPosition);
}

// --------------------------------------------------------------------------
void ctk3Slider::setPositions(int min, int mid, int max)
{
    Q_D(ctk3Slider);
    sortLMU(min, mid, max);

    // Can't bound with this->minimum() or maximum() when the system is zoomed in.
    const int minPosition = min;
    const int maxPosition = max;
    const int midPosition = mid;

    bool emitMinPosChanged = (minPosition != d->m_MinimumPosition);
    bool emitMaxPosChanged = (maxPosition != d->m_MaximumPosition);
    bool emitMidPosChanged = (midPosition != d->m_MidPosition);

    if (!emitMinPosChanged && !emitMaxPosChanged && !emitMidPosChanged)
    {
        return;
    }

    d->m_MinimumPosition = minPosition;
    d->m_MaximumPosition = maxPosition;
    d->m_MidPosition = midPosition;

    if (!this->hasTracking())
    {
        this->update();
    }
    if (isSliderDown())
    {
        if (emitMinPosChanged)
        {
            emit minimumPositionChanged(d->m_MinimumPosition);
        }
        if (emitMaxPosChanged)
        {
            emit maximumPositionChanged(d->m_MaximumPosition);
        }
        if (emitMidPosChanged)
        {
            emit midPositionChanged(d->m_MidPosition);
        }
        if (emitMinPosChanged || emitMaxPosChanged || emitMidPosChanged)
        {
            emit positionsChanged(d->m_MinimumPosition, d->m_MidPosition, d->m_MaximumPosition);
        }
    }
    if (this->hasTracking())
    {
        this->triggerAction(SliderMove);
        this->setValues(d->m_MinimumPosition, d->m_MidPosition, d->m_MaximumPosition);
    }
}

// --------------------------------------------------------------------------
void ctk3Slider::onRangeChanged(int _minimum, int _maximum)
{
    Q_UNUSED(_minimum);
    Q_UNUSED(_maximum);
    Q_D(ctk3Slider);
}

// --------------------------------------------------------------------------
// Render
void ctk3Slider::paintEvent( QPaintEvent* )
{
    Q_D(ctk3Slider);
    QStyleOptionSlider option;
    this->initStyleOption(&option);

    QStylePainter painter(this);
    option.subControls = QStyle::SC_SliderGroove;
    // Move to minimum to not highlight the SliderGroove.
    // On mac style, drawing just the slider groove also draws the handles,
    // therefore we give a negative (outside of view) position.
    option.sliderValue = this->minimum() - this->maximum();
    option.sliderPosition = this->minimum() - this->maximum();
    painter.drawComplexControl(QStyle::CC_Slider, option);

    option.sliderPosition = d->m_MinimumPosition;
    const QRect lr = style()->subControlRect( QStyle::CC_Slider,
                     &option,
                     QStyle::SC_SliderHandle,
                     this);

    option.sliderPosition = d->m_MidPosition;

    const QRect mr = style()->subControlRect( QStyle::CC_Slider,
                     &option,
                     QStyle::SC_SliderHandle,
                     this);

    option.sliderPosition = d->m_MaximumPosition;

    const QRect ur = style()->subControlRect( QStyle::CC_Slider,
                     &option,
                     QStyle::SC_SliderHandle,
                     this);

    QRect sr = style()->subControlRect( QStyle::CC_Slider,
                                        &option,
                                        QStyle::SC_SliderGroove,
                                        this);
    QRect rangeBox;
    if (option.orientation == Qt::Horizontal)
    {
        rangeBox = QRect(
                       QPoint(qMin( lr.center().x(), ur.center().x() ), sr.center().y() - 2),
                       QPoint(qMax( lr.center().x(), ur.center().x() ), sr.center().y() + 1));
    }
    else
    {
        rangeBox = QRect(
                       QPoint(sr.center().x() - 2, qMin( lr.center().y(), ur.center().y() )),
                       QPoint(sr.center().x() + 1, qMax( lr.center().y(), ur.center().y() )));
    }

    // -----------------------------
    // Render the range
    //
    QRect groove = this->style()->subControlRect( QStyle::CC_Slider,
                   &option,
                   QStyle::SC_SliderGroove,
                   this );
    groove.adjust(0, 0, -1, 0);

    // Create default colors based on the transfer function.
    //
    QColor highlight = this->palette().color(QPalette::Normal, QPalette::Highlight);
    QLinearGradient gradient;
    if (option.orientation == Qt::Horizontal)
    {
        gradient = QLinearGradient( groove.center().x(), groove.top(),
                                    groove.center().x(), groove.bottom());
    }
    else
    {
        gradient = QLinearGradient( groove.left(), groove.center().y(),
                                    groove.right(), groove.center().y());
    }

    gradient.setColorAt(0, highlight.darker(120));
    gradient.setColorAt(1, highlight.lighter(160));

    painter.setPen(QPen(highlight.darker(150), 0));
    painter.setBrush(gradient);
    painter.drawRect( rangeBox.intersected(groove) );

    //  -----------------------------------
    // Render the sliders
    //
    if (this->isMinimumSliderDown())
    {
        d->drawMaximumSlider( &painter );
        d->drawMidSlider( &painter );
        d->drawMinimumSlider( &painter );
    }
    else if (this->isMidSliderDown())
    {
        d->drawMaximumSlider( &painter );
        d->drawMinimumSlider( &painter );
        d->drawMidSlider( &painter );
    }
    else
    {
        d->drawMinimumSlider( &painter );
        d->drawMidSlider( &painter );
        d->drawMaximumSlider( &painter );
    }
}

// --------------------------------------------------------------------------
// Standard Qt UI events
void ctk3Slider::mousePressEvent(QMouseEvent* mouseEvent)
{
    Q_D(ctk3Slider);
    if (minimum() == maximum() || (mouseEvent->buttons() ^ mouseEvent->button()))
    {
        mouseEvent->ignore();
        return;
    }
    int mepos = this->orientation() == Qt::Horizontal ?
                mouseEvent->pos().x() : mouseEvent->pos().y();

    QStyleOptionSlider option;
    this->initStyleOption( &option );

    QRect handleRect;
    ctk3SliderPrivate::Handle handle_ = d->handleAtPos(mouseEvent->pos(), handleRect);

    if (handle_ != ctk3SliderPrivate::NoHandle)
    {
        if (handle_ == ctk3SliderPrivate::MinimumHandle)
            d->m_SubclassPosition = d->m_MinimumPosition;
        else if (handle_ == ctk3SliderPrivate::MaximumHandle)
            d->m_SubclassPosition = d->m_MaximumPosition;
        else if (handle_ == ctk3SliderPrivate::MidHandle)
            d->m_SubclassPosition = d->m_MidPosition;

        // save the position of the mouse inside the handle for later
        d->m_SubclassClickOffset = mepos - (this->orientation() == Qt::Horizontal ?
                                            handleRect.left() : handleRect.top());

        this->setSliderDown(true);

        if (d->m_SelectedHandles != handle_)
        {
            d->m_SelectedHandles = handle_;
            this->update(handleRect);
        }
        // Accept the mouseEvent
        mouseEvent->accept();
        return;
    }

    // if we are here, no handles have been pressed
    // Check if we pressed on the groove between the 2 handles

    QStyle::SubControl control = this->style()->hitTestComplexControl(
                                     QStyle::CC_Slider, &option, mouseEvent->pos(), this);
    QRect sr = style()->subControlRect(
                   QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
    int minCenter = (this->orientation() == Qt::Horizontal ?
                     handleRect.left() : handleRect.top());
    int maxCenter = (this->orientation() == Qt::Horizontal ?
                     handleRect.right() : handleRect.bottom());
    if (control == QStyle::SC_SliderGroove &&
            mepos > minCenter && mepos < maxCenter)
    {
        // warning lost of precision it might be fatal
        d->m_SubclassPosition = (d->m_MinimumPosition + d->m_MaximumPosition) / 2.;
        d->m_SubclassClickOffset = mepos - d->pixelPosFromRangeValue(d->m_SubclassPosition);
        d->m_SubclassWidth = (d->m_MaximumPosition - d->m_MinimumPosition) / 2;
        qMax(d->m_SubclassPosition - d->m_MinimumPosition, d->m_MaximumPosition - d->m_SubclassPosition);
        this->setSliderDown(true);
        if (!this->isMinimumSliderDown() || !this->isMaximumSliderDown())
        {
            d->m_SelectedHandles =
                QFlags<ctk3SliderPrivate::Handle>(ctk3SliderPrivate::MinimumHandle) |
                QFlags<ctk3SliderPrivate::Handle>(ctk3SliderPrivate::MaximumHandle);
            this->update(handleRect.united(sr));
        }
        mouseEvent->accept();
        return;
    }
    mouseEvent->ignore();
}

// --------------------------------------------------------------------------
// Standard Qt UI events
void ctk3Slider::mouseMoveEvent(QMouseEvent* mouseEvent)
{
    Q_D(ctk3Slider);
    if (!d->m_SelectedHandles)
    {
        mouseEvent->ignore();
        return;
    }
    int mepos = this->orientation() == Qt::Horizontal ?
                mouseEvent->pos().x() : mouseEvent->pos().y();

    QStyleOptionSlider option;
    this->initStyleOption(&option);

    const int m = style()->pixelMetric( QStyle::PM_MaximumDragDistance, &option, this );

    int newPosition = d->pixelPosToRangeValue(mepos - d->m_SubclassClickOffset);

    if (m >= 0)
    {
        const QRect r = rect().adjusted(-m, -m, m, m);
        if (!r.contains(mouseEvent->pos()))
        {
            newPosition = d->m_SubclassPosition;
        }
    }

    // If two sliders down, pick the one that makes sense.
    // TODO: this won't trigger because right not the code doesn't recognize two sliders down.
    if (this->isMinimumSliderDown() && this->isMidSliderDown())
    {
        // To break the tie, first try min, but only if the new position
        // isn't > the mid position
        if (newPosition < d->m_MidPosition)
            this->setPositions(newPosition, d->m_MidPosition, d->m_MaximumPosition);
        else
            this->setPositions(d->m_MinimumPosition, newPosition, d->m_MaximumPosition);
    }
    else if (this->isMidSliderDown() && this->isMaximumSliderDown())
    {
        // To break the tie, first try min, but only if the new position
        // isn't > the mid position
        if (newPosition < d->m_MaximumPosition)
            this->setPositions(d->m_MinimumPosition, newPosition, d->m_MaximumPosition);
        else
            this->setPositions(d->m_MinimumPosition, d->m_MidPosition, newPosition);
    }
    else if (this->isMinimumSliderDown())
    {
        double newMinPos = qMin(newPosition, d->m_MidPosition);
        this->setPositions(newMinPos, d->m_MidPosition, d->m_MaximumPosition);
    }
    // The upper/right slider is down
    else if (this->isMaximumSliderDown())
    {
        double newMaxPos = qMax(d->m_MidPosition, newPosition);
        this->setPositions(d->m_MinimumPosition, d->m_MidPosition, newMaxPos);
    }
    // The mid slider is down
    else if (this->isMidSliderDown())
    {
        double newMidPos = qMin(qMax(d->m_MinimumPosition, newPosition), d->m_MaximumPosition);
        this->setPositions(d->m_MinimumPosition, newMidPos, d->m_MaximumPosition);
    }
    mouseEvent->accept();
}

// --------------------------------------------------------------------------
// Standard Qt UI mouseEvents
void ctk3Slider::mouseReleaseEvent(QMouseEvent* mouseEvent)
{
    Q_D(ctk3Slider);
    this->QSlider::mouseReleaseEvent(mouseEvent);

    setSliderDown(false);
    d->m_SelectedHandles = ctk3SliderPrivate::NoHandle;

    emit released(d->m_MinimumValue, d->m_MidValue, d->m_MaximumValue);
    this->update();
}

// --------------------------------------------------------------------------
bool ctk3Slider::isMinimumSliderDown()const
{
    Q_D(const ctk3Slider);
    return d->m_SelectedHandles & ctk3SliderPrivate::MinimumHandle;
}

// --------------------------------------------------------------------------
bool ctk3Slider::isMaximumSliderDown()const
{
    Q_D(const ctk3Slider);
    return d->m_SelectedHandles & ctk3SliderPrivate::MaximumHandle;
}
// --------------------------------------------------------------------------
bool ctk3Slider::isMidSliderDown()const
{
    Q_D(const ctk3Slider);
    return d->m_SelectedHandles & ctk3SliderPrivate::MidHandle;
}

// --------------------------------------------------------------------------
void ctk3Slider::initMinimumSliderStyleOption(QStyleOptionSlider* option) const
{
    this->initStyleOption(option);
}

// --------------------------------------------------------------------------
void ctk3Slider::initMaximumSliderStyleOption(QStyleOptionSlider* option) const
{
    this->initStyleOption(option);
}
// --------------------------------------------------------------------------
void ctk3Slider::initMidSliderStyleOption(QStyleOptionSlider* option) const
{
    this->initStyleOption(option);
}

// --------------------------------------------------------------------------
QString ctk3Slider::handleToolTip()const
{
    Q_D(const ctk3Slider);
    return d->m_HandleToolTip;
}

// --------------------------------------------------------------------------
void ctk3Slider::setHandleToolTip(const QString &_toolTip)
{
    Q_D(ctk3Slider);
    d->m_HandleToolTip = _toolTip;
}

// --------------------------------------------------------------------------
bool ctk3Slider::event(QEvent* _event)
{
    Q_D(ctk3Slider);
    switch(_event->type())
    {
        case QEvent::ToolTip:
        {
            QHelpEvent* helpEvent = static_cast<QHelpEvent*>(_event);
            QStyleOptionSlider opt;
            // Test the MinimumHandle
            opt.sliderPosition = d->m_MinimumPosition;
            opt.sliderValue = d->m_MinimumValue;
            this->initStyleOption(&opt);
            QStyle::SubControl hoveredControl =
                this->style()->hitTestComplexControl(
                    QStyle::CC_Slider, &opt, helpEvent->pos(), this);
            if (!d->m_HandleToolTip.isEmpty() &&
                    hoveredControl == QStyle::SC_SliderHandle)
            {
                QToolTip::showText(helpEvent->globalPos(), d->m_HandleToolTip.arg(this->minimumValue()));
                _event->accept();
                return true;
            }
            // Test the MaximumHandle
            opt.sliderPosition = d->m_MaximumPosition;
            opt.sliderValue = d->m_MaximumValue;
            this->initStyleOption(&opt);
            hoveredControl = this->style()->hitTestComplexControl(
                                 QStyle::CC_Slider, &opt, helpEvent->pos(), this);
            if (!d->m_HandleToolTip.isEmpty() &&
                    hoveredControl == QStyle::SC_SliderHandle)
            {
                QToolTip::showText(helpEvent->globalPos(), d->m_HandleToolTip.arg(this->maximumValue()));
                _event->accept();
                return true;
            }
            // Test the MidHandle
            opt.sliderPosition = d->m_MidPosition;
            opt.sliderValue = d->m_MidValue;
            this->initStyleOption(&opt);
            hoveredControl = this->style()->hitTestComplexControl(
                                 QStyle::CC_Slider, &opt, helpEvent->pos(), this);
            if (!d->m_HandleToolTip.isEmpty() &&
                    hoveredControl == QStyle::SC_SliderHandle)
            {
                QToolTip::showText(helpEvent->globalPos(), d->m_HandleToolTip.arg(this->midValue()));
                _event->accept();
                return true;
            }
        }
        default:
            break;
    }
    return this->Superclass::event(_event);
}
