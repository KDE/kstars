/*
    SPDX-FileCopyrightText: 2009 Khudyakov Alexey <alexey.skladnoy@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QPoint>
#include <QList>
#include <QString>
#include <QStringList>

class SkyPoint;
class SkyObject;
class InfoBoxWidget;

/**
 * @brief The InfoBoxes class is a collection of InfoBoxWidget objects that display a transparent box for display of text messages
 */
class InfoBoxes : public QWidget
{
    Q_OBJECT

  public:
    explicit InfoBoxes(QWidget *parent = nullptr);

    virtual ~InfoBoxes() override = default;

    void addInfoBox(InfoBoxWidget *ibox);
    QList<InfoBoxWidget *> getInfoBoxes() const { return m_boxes; }

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private:
    QList<InfoBoxWidget *> m_boxes;
};

/**
* @brief The InfoBoxWidget class is a widget that displays a transparent box for display of text messages.
*/
class InfoBoxWidget : public QWidget
{
    Q_OBJECT
  public:
    /** Alignment of widget. */
    enum
    {
        NoAnchor     = 0,
        AnchorRight  = 1,
        AnchorBottom = 2,
        AnchorBoth   = 3
    };

    /** Create one infobox. */
    InfoBoxWidget(bool shade, const QPoint &pos, int anchor = 0, const QStringList &str = QStringList(),
                  QWidget *parent = nullptr);
    /** Destructor */
    virtual ~InfoBoxWidget() override = default;

    /** Check whether box is shaded. In this case only one line is shown. */
    bool shaded() const { return m_shaded; }
    /** Get stickyness status of */
    int sticky() const { return m_anchor; }

    /** Adjust widget's position */
    void adjust();

  public slots:
    /** Set information about time. Data is taken from KStarsData. */
    void slotTimeChanged();
    /** Set information about location. Data is taken from KStarsData. */
    void slotGeoChanged();
    /** Set information about object. */
    void slotObjectChanged(SkyObject *obj);
    /** Set information about pointing. */
    void slotPointChanged(SkyPoint *p);
  signals:
    /** Emitted when widget is clicked */
    void clicked();

  protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    /** Uset to set information about object. */
    void setPoint(QString name, SkyPoint *p);
    /** Recalculate size of widget */
    void updateSize();

    /// List of string to show
    QStringList m_strings;
    /// True if widget coordinates were adjusted
    bool m_adjusted { true };
    /// True if widget is dragged around
    bool m_grabbed { true };
    /// True if widget if shaded
    bool m_shaded { true };
    /// Vertical alignment of widget
    int m_anchor { 0 };

    static const int padX;
    static const int padY;
};
