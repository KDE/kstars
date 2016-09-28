/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef ScrollGraph_H_
#define ScrollGraph_H_

#include <QtGui>
#include <QWidget>
#include <QImage>
#include <QPainter>


#define RA_LINE    0
#define DEC_LINE   1

typedef struct
{
    double *line[2];
}delta_data_t;



class ScrollGraph : public QWidget
{
public:
    ScrollGraph(QWidget *parent);
    virtual ~ScrollGraph();

    //QImage *get_buffer( void );
    void setSize(int w, int h);
    bool addPoint( double ra, double dec );
    void setVisibleRanges( int rx, int ry );
    void getVisibleRanges( int *rx, int *ry );
    int  getGridN( void );
    void resetView( void );
    void resetData( void );
    void getScreenSize( int *sx, int *sy );
    void setRAEnabled(bool enable) { RAEnabled = enable; }
    void setDEEnabled(bool enable) { DEEnabled = enable; }

protected:
    void paintEvent( QPaintEvent * );
    void resizeEvent(QResizeEvent *event);

private:
    QColor BKGD_COLOR, RA_COLOR, DEC_COLOR, GRID_COLOR, WHITE_COLOR, GRID_FONT_COLOR;
    QPen pen;
    QBrush brush;
    int half_buffer_size_wd;
    int half_buffer_size_ht;
    int client_rect_wd;
    int client_rect_ht;
    bool need_refresh;

    // data
    delta_data_t data;
    int data_cnt;
    int	data_idx;
    int grid_N;

    // grid vars...
    double grid_step_x, grid_step_y, grid_view_step_x, grid_view_step_y;
    int font_ht_k;

    // control
    int vis_range_x, vis_range_y;
    int half_vis_range_x, half_vis_range_y;

    bool RAEnabled, DEEnabled;

    void refresh( void );
    void drawGrid(QPainter *p, double kx, double ky );
    void initRenderVariables( void );

};

#endif /*ScrollGraph_H_*/
