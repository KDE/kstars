/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#ifndef COMMON_H_
#define COMMON_H_

#include <QPainter>
#include <QMouseEvent>
#include <QWidget>

#include "../indi/indicommon.h"

extern const uint8_t DEF_BKGD_COLOR[];
extern const uint8_t DEF_RA_COLOR[];
extern const uint8_t DEF_DEC_COLOR[];
extern const uint8_t DEF_GRID_COLOR[];
extern const uint8_t DEF_WHITE_COLOR[];
extern const uint8_t DEF_GRID_FONT_COLOR[];

extern const uint8_t DEF_SQR_OVL_COLOR[];

void u_msg( const char *fmt, ...);

struct guide_dir_desc
{
    GuideDirection dir;
    const char desc[10];
};

typedef struct
{
    int x, y;
}point_t;

static const int LOOP_DELAY	= 10000;

class mouse_delegate
{
public:
    virtual void mouse_press( QMouseEvent *event ) = 0;
    virtual void mouse_release( QMouseEvent *event ) = 0;
    virtual void mouse_move( QMouseEvent *event ) = 0;

    virtual ~mouse_delegate() {}
};


class custom_drawer : public QWidget
{
    Q_OBJECT

public:
    explicit custom_drawer(QWidget *parent = NULL ) : QWidget(parent), m_mouse(NULL), m_image(NULL)
    {
    }
    ~custom_drawer()
    {
    }
    bool set_source( QImage *image, mouse_delegate *mouse )
    {
        m_image = image;
        if( !m_image )
            return false;
        resize( m_image->size() );
        m_mouse = mouse;
     return true;
    }
protected:
    void paintEvent(QPaintEvent *)
    {
        if( !m_image )
            return;
        QPainter painter;
        painter.begin(this);
        painter.drawImage( 0, 0, *m_image );
        painter.end();
    };

    void mouseMoveEvent ( QMouseEvent *event )
    {
        if( !m_mouse )
            return;
        m_mouse->mouse_move( event );
    }
    void mousePressEvent ( QMouseEvent *event )
    {
        if( !m_mouse )
            return;
        m_mouse->mouse_press( event );
    }
    void mouseReleaseEvent ( QMouseEvent *event )
    {
        if( !m_mouse )
            return;
        m_mouse->mouse_release( event );
    }
private:
    mouse_delegate *m_mouse;
    QImage *m_image;
};


#endif /* COMMON_H_ */
