/*
    SPDX-FileCopyrightText: Kitware Inc.
    SPDX-License-Identifier: Apache-2.0

    Modified from the original code to support 3 sliders, May 2023.
*/


#ifndef __ctk3Slider_h
#define __ctk3Slider_h

#include <QSlider>

class QStylePainter;
class ctk3SliderPrivate;

/// \ingroup Widgets
///
/// A ctk3Slider is a slider that lets you input 2 values instead of one
/// (see QSlider). These values are typically a lower and upper bound.
/// Values are comprised between the range of the slider. See setRange(),
/// minimum() and maximum(). The upper bound can't be smaller than the
/// lower bound and vice-versa.
/// When setting new values (setMinimumValue(), setMaximumValue() or
/// setValues()), make sure they lie between the range (minimum(), maximum())
/// of the slider, they would be forced otherwised. If it is not the behavior
/// you desire, you can set the range first (setRange(), setMinimum(),
/// setMaximum())
/// \sa ctkDoubleRangeSlider, ctkDoubleSlider, ctkRangeWidget
class ctk3Slider : public QSlider
{
        Q_OBJECT
        Q_PROPERTY(int minimumValue READ minimumValue WRITE setMinimumValue)
        Q_PROPERTY(int maximumValue READ maximumValue WRITE setMaximumValue)
        Q_PROPERTY(int midValue READ midValue WRITE setMidValue)
        Q_PROPERTY(int minimumPosition READ minimumPosition WRITE setMinimumPosition)
        Q_PROPERTY(int maximumPosition READ maximumPosition WRITE setMaximumPosition)
        Q_PROPERTY(int midPosition READ midPosition WRITE setMidPosition)
        Q_PROPERTY(QString handleToolTip READ handleToolTip WRITE setHandleToolTip)

    public:
        // Superclass typedef
        typedef QSlider Superclass;
        /// Constructor, builds a ctk3Slider that ranges from 0 to 100 and has
        /// a lower and upper values of 0 and 100 respectively, other properties
        /// are set the QSlider default properties.
        explicit ctk3Slider( Qt::Orientation o, QWidget* par = 0 );
        explicit ctk3Slider( QWidget* par = 0 );
        virtual ~ctk3Slider();

        ///
        /// This property holds the slider's current minimum value.
        /// The slider silently forces minimumValue to be within the legal range:
        /// minimum() <= minimumValue() <= maximumValue() <= maximum().
        /// Changing the minimumValue also changes the minimumPosition.
        int minimumValue() const;

        ///
        /// This property holds the slider's current maximum value.
        /// The slider forces the maximum value to be within the legal range:
        /// The slider silently forces maximumValue to be within the legal range:
        /// Changing the maximumValue also changes the maximumPosition.
        int maximumValue() const;

        ///
        /// This property holds the slider's current mid value.
        /// The slider forces the mid value to be within the legal range:
        /// The slider silently forces midValue to be within the legal range:
        /// Changing the midValue also changes the midPosition.
        int midValue() const;

        ///
        /// This property holds the current slider minimum position.
        /// If tracking is enabled (the default), this is identical to minimumValue.
        int minimumPosition() const;
        void setMinimumPosition(int min);

        ///
        /// This property holds the current slider maximum position.
        /// If tracking is enabled (the default), this is identical to maximumValue.
        int maximumPosition() const;
        void setMaximumPosition(int max);

        ///
        /// This property holds the current slider mid position.
        /// If tracking is enabled (the default), this is identical to midValue.
        int midPosition() const;
        void setMidPosition(int mid);

        ///
        /// Utility function that set the minimum, mid and maximum position at once.
        void setPositions(int min, int mid, int max);

        ///
        /// Controls the text to display for the handle tooltip. It is in addition
        /// to the widget tooltip.
        /// "%1" is replaced by the current value of the slider.
        /// Empty string (by default) means no tooltip.
        QString handleToolTip()const;
        void setHandleToolTip(const QString &toolTip);

        /// Returns true if the minimum value handle is down, false if it is up.
        /// \sa isMaximumSliderDown()
        bool isMinimumSliderDown()const;
        /// Returns true if the maximum value handle is down, false if it is up.
        /// \sa isMinimumSliderDown()
        bool isMaximumSliderDown()const;
        /// Returns true if the mid value handle is down, false if it is up.
        /// \sa isMinimumSliderDown()
        bool isMidSliderDown()const;

    Q_SIGNALS:
        ///
        /// This signal is emitted when the slider minimum value has changed,
        /// with the new slider value as argument.
        void minimumValueChanged(int min);
        ///
        /// This signal is emitted when the slider maximum value has changed,
        /// with the new slider value as argument.
        void maximumValueChanged(int max);
        ///
        /// This signal is emitted when the slider mid value has changed,
        /// with the new slider value as argument.
        void midValueChanged(int mid);
        ///
        /// Utility signal that is fired when minimum, mid or maximum values have changed.
        void valuesChanged(int min, int mid, int max);

        ///
        /// This signal is emitted when sliderDown is true and the slider moves.
        /// This usually happens when the user is dragging the minimum slider.
        /// The value is the new slider minimum position.
        /// This signal is emitted even when tracking is turned off.
        void minimumPositionChanged(int min);

        ///
        /// This signal is emitted when sliderDown is true and the slider moves.
        /// This usually happens when the user is dragging the maximum slider.
        /// The value is the new slider maximum position.
        /// This signal is emitted even when tracking is turned off.
        void maximumPositionChanged(int max);

        ///
        /// This signal is emitted when sliderDown is true and the slider moves.
        /// This usually happens when the user is dragging the mid slider.
        /// The value is the new slider mid position.
        /// This signal is emitted even when tracking is turned off.
        void midPositionChanged(int max);

        ///
        /// Utility signal that is fired when minimum or maximum positions
        /// have changed.
        void positionsChanged(int min, int mid, int max);

        // Emitted when the mouse is let go.
        void released(int min, int mid, int max);

    public Q_SLOTS:
        ///
        /// This property holds the slider's current minimum value.
        /// The slider silently forces min to be within the legal range:
        /// minimum() <= min <= mid <= maximumValue() <= maximum().
        /// Note: Changing the minimumValue also changes the minimumPosition.
        /// \sa stMaximumValue, setValues, setMinimum, setMaximum, setRange
        void setMinimumValue(int min);

        ///
        /// This property holds the slider's current maximum value.
        /// The slider silently forces max to be within the legal range:
        /// minimum() <= minimumValue() <= mid <= max <= maximum().
        /// Note: Changing the maximumValue also changes the maximumPosition.
        /// \sa stMinimumValue, setValues, setMinimum, setMaximum, setRange
        void setMaximumValue(int max);

        ///
        /// This property holds the slider's current mid value.
        /// The slider silently forces max to be within the legal range:
        /// minimum() <= minimumValue() <= mid <= max <= maximum().
        /// Note: Changing the midValue also changes the midPosition.
        /// \sa setMidValue, setValues, setMinimum, setMaximum, setRange
        void setMidValue(int mid);

        ///
        /// Utility function that set the minimum value and maximum value at once.
        /// The slider silently forces min and max to be within the legal range:
        /// minimum() <= min <= max <= maximum().
        /// Note: Changing the minimumValue and maximumValue also changes the
        /// minimumPosition and maximumPosition.
        /// \sa setMinimumValue, setMaximumValue, setMinimum, setMaximum, setRange
        void setValues(int min, int mid, int max);

    protected Q_SLOTS:
        void onRangeChanged(int minimum, int maximum);

    protected:
        ctk3Slider( ctk3SliderPrivate* impl, Qt::Orientation o, QWidget* par = 0 );
        ctk3Slider( ctk3SliderPrivate* impl, QWidget* par = 0 );

        // Description:
        // Standard Qt UI events
        virtual void mousePressEvent(QMouseEvent* ev) override;
        virtual void mouseMoveEvent(QMouseEvent* ev) override;
        virtual void mouseReleaseEvent(QMouseEvent* ev) override;

        // Description:
        // Rendering is done here.
        virtual void paintEvent(QPaintEvent* ev) override;
        virtual void initMinimumSliderStyleOption(QStyleOptionSlider* option) const;
        virtual void initMaximumSliderStyleOption(QStyleOptionSlider* option) const;
        virtual void initMidSliderStyleOption(QStyleOptionSlider* option) const;

        // Description:
        // Reimplemented for the tooltips
        virtual bool event(QEvent* event) override;

    protected:
        QScopedPointer<ctk3SliderPrivate> d_ptr;

    private:
        Q_DECLARE_PRIVATE(ctk3Slider);
        Q_DISABLE_COPY(ctk3Slider);
};

#endif

