/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef CSCROLL_GRAPH_H_
#define CSCROLL_GRAPH_H_

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



class cscroll_graph
{
public:
	cscroll_graph( QWidget *own, int client_width, int client_height );
	virtual ~cscroll_graph();
	
	QImage *get_buffer( void );
	bool add_point( double ra, double dec );
	void set_visible_ranges( int rx, int ry );
	void get_visible_ranges( int *rx, int *ry );
	int  get_grid_N( void );
	void reset_view( void );
	void reset_data( void );
	void on_paint( void );
	void get_screen_size( int *sx, int *sy );
	
	
private:
	// view
	QWidget *owner;
	QImage *buffer;
	QPainter canvas;
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
	
	void refresh( void );
	void draw_grid( double kx, double ky );
	void init_render_vars( void );
	
};

#endif /*CSCROLL_GRAPH_H_*/
