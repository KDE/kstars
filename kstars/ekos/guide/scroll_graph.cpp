/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QtGui>
#include <QWidget>

#include "scroll_graph.h"
#include "common.h"



cscroll_graph::cscroll_graph( QWidget *own, int client_width, int client_height )
{
	owner = own;

	client_rect_wd = client_width;
	client_rect_ht = client_height;

	buffer = new QImage( client_rect_wd, client_rect_ht, QImage::Format_RGB32 );

	vis_range_x = client_rect_wd; // horizontal range in ticks
	vis_range_y = 100;	// whole visible vertical range in arcsecs!

	grid_N = 6;

	data_cnt = 10*grid_N*10;
	data.line[ RA_LINE ] = new double[ data_cnt ];
	data.line[ DEC_LINE ] = new double[ data_cnt ];
	reset_data();

	//graphics...
	pen.setStyle( Qt::SolidLine );
	pen.setWidth(1);
	brush.setStyle(Qt::SolidPattern);

	RA_COLOR 		= QColor( DEF_RA_COLOR[0], DEF_RA_COLOR[1], DEF_RA_COLOR[2] );
	DEC_COLOR 		= QColor( DEF_DEC_COLOR[0], DEF_DEC_COLOR[1], DEF_DEC_COLOR[2] );
	GRID_COLOR 		= QColor( DEF_GRID_COLOR[0], DEF_GRID_COLOR[1], DEF_GRID_COLOR[2] );
	BKGD_COLOR 		= QColor( DEF_BKGD_COLOR[0], DEF_BKGD_COLOR[1], DEF_BKGD_COLOR[2] );
	WHITE_COLOR 	= QColor( DEF_WHITE_COLOR[0], DEF_WHITE_COLOR[1], DEF_WHITE_COLOR[2] );
	GRID_FONT_COLOR	= QColor( DEF_GRID_FONT_COLOR[0], DEF_GRID_FONT_COLOR[1], DEF_GRID_FONT_COLOR[2] );
	brush.setColor( BKGD_COLOR );


	// init...
	init_render_vars();

	need_refresh = true;


}

cscroll_graph::~cscroll_graph()
{
	delete buffer;
	delete [] data.line[ RA_LINE ];
	delete [] data.line[ DEC_LINE ];
}


void cscroll_graph::init_render_vars( void )
{
	half_buffer_size_wd = client_rect_wd / 2;
	half_buffer_size_ht = client_rect_ht / 2;


	grid_view_step_x = (double)client_rect_wd / (double)grid_N;
	grid_view_step_y = (double)client_rect_ht / (double)grid_N;

	grid_step_x = (double)vis_range_x / (double)grid_N;
	grid_step_y = (double)vis_range_y / (double)grid_N;

	half_vis_range_x = vis_range_x / 2;
	half_vis_range_y = vis_range_y / 2;

}


void cscroll_graph::set_visible_ranges( int rx, int ry )
{
     if( rx >= 10*grid_N && rx < (double)data_cnt )
     {
         if( vis_range_x != rx )
             need_refresh = true;
         vis_range_x = rx;
     }
     else
     {
         u_msg("set_visible_ranges: must be >= %d and < %d", 10*grid_N, data_cnt);
         return;
     }

     if( ry >= 5*grid_N )
     {
         if( vis_range_x != ry )
        	 need_refresh = true;
         vis_range_y = ry;
     }
     else
     {
         u_msg("set_visible_ranges: must be >= %d and < %d", 5*grid_N, data_cnt);
         return;
     }

     init_render_vars();
}


void cscroll_graph::get_visible_ranges( int *rx, int *ry )
{
	*rx = vis_range_x;
	*ry = vis_range_y;
}


int cscroll_graph::get_grid_N( void )
{
 return grid_N;
}


void cscroll_graph::reset_view( void )
{

	set_visible_ranges( client_rect_wd, 100 );

	init_render_vars();

    need_refresh = true;
}


void cscroll_graph::reset_data( void )
{
	memset( data.line[RA_LINE], 0, sizeof(double)*data_cnt );
	memset( data.line[DEC_LINE], 0, sizeof(double)*data_cnt );
	data_idx = 0;
}



QImage *cscroll_graph::get_buffer( void )
{
 return buffer;
}


void cscroll_graph::get_screen_size( int *sx, int *sy )
{
	*sx = client_rect_wd;
	*sy = client_rect_ht;
}


void cscroll_graph::on_paint( void )
{
	canvas.begin( buffer );

	refresh();

	canvas.end();
}


/*************
*
* Main Drawing function
*
**************/
void cscroll_graph::refresh( void )
{
 int i, j, k;
 double kx, ky, step;
 double *data_ptr;
 int start_idx;
 int /*band1_wd,*/ band1_start, band1_end;
 int band2_wd, band2_start, band2_end;
 int x, y;
 int px, py;

	if( !need_refresh )
	    return;

	font_ht_k = canvas.fontMetrics().ascent();

	// fill background
	canvas.fillRect( 0, 0, client_rect_wd, client_rect_ht, brush);

	start_idx = (data_idx + data_cnt - vis_range_x) % data_cnt;
	// split visible region in 2 ranges
	if( data_idx > start_idx ) // only 1 band
	{
		//band1_wd 	= data_idx - start_idx; // = vis_range_x
		band1_start = start_idx;
		band1_end	= data_idx; // -1;
		band2_start = band2_end = band2_wd = 0;
	}
	else // 2 bands
	{
		//band1_wd 	= data_idx;
		band1_start = 0;
		band1_end 	= data_idx; //-1;

		band2_wd 	= data_cnt - start_idx;
		band2_start = start_idx;
		band2_end	= data_cnt-1;
	}

	// Rasterizing coefficients
	kx = (double)client_rect_wd / vis_range_x;
	ky = (double)client_rect_ht / vis_range_y;

	draw_grid( kx, ky );

	// analize kx and select optimal algorithm
	if( client_rect_wd <= vis_range_x )
	{
		step = 1.0 / kx;

		for( k = 0;k < 2;k++ )
		{
			data_ptr = data.line[k];

			if( k == RA_LINE )
				pen.setColor( RA_COLOR );
			else
				pen.setColor( DEC_COLOR );

			canvas.setPen( pen );

			// process band 1
			px = client_rect_wd;;
			py = half_buffer_size_ht - (int)(data_ptr[band1_end] * ky);

			x = client_rect_wd;

			for( i = band1_end, j = 0;i >= band1_start; )
			{
				y = half_buffer_size_ht - (int)(data_ptr[i] * ky);
				x--;

				canvas.drawLine( px, py, x, y );

				px = x;
				py = y;

				//------------------------------------------
				++j;
				i = band1_end - (int)((double)j*step);
			}

			// process band 2
			for( i = band2_end, j = 0;i > band2_start; )
			{
				y = half_buffer_size_ht - (int)(data_ptr[i] * ky);
				x--;

				canvas.drawLine( px, py, x, y );

				px = x;
				py = y;

				//------------------------------------------
				++j;
				i = band2_end - (int)((double)j*step);
			}
		}
	}
	else
	{
		step = kx;

		for( k = 0;k < 2;k++ )
		{
			data_ptr = data.line[k];

			if( k == RA_LINE )
				pen.setColor( RA_COLOR );
			else
				pen.setColor( DEC_COLOR );

			canvas.setPen( pen );

			// process band 1
			px = client_rect_wd;
			py = half_buffer_size_ht - (int)(data_ptr[band1_end] * ky);

			x = client_rect_wd;

			for( i = band1_end, j = 0;i >= band1_start;i--, ++j )
			{
				y = half_buffer_size_ht - (int)(data_ptr[i] * ky);
				x = client_rect_wd - (int)((double)j*step) - 1;

				canvas.drawLine( px, py, x, y );

				px = x;
				py = y;

			}


			// process band 2
			for( i = band2_end;i > band2_start;i--, ++j )
			{
				y = half_buffer_size_ht - (int)(data_ptr[i] * ky);
				x = client_rect_wd - (int)((double)j*step) - 1;

				canvas.drawLine( px, py, x, y );

				px = x;
				py = y;
			}

		}
	}

	need_refresh = false;
}


void cscroll_graph::draw_grid( double kx, double )
{
 int i, x, sx, y;
 int grid_column, val;
 QString str;

	pen.setColor( GRID_COLOR );
	canvas.setPen( pen );

	grid_column = data_idx / (int)grid_step_x * (int)grid_step_x;
	sx = client_rect_wd - (double)(data_idx % (int)grid_step_x)*kx;

	for( i = 0;i < grid_N;++i )
	{
		x = sx - (double)i*grid_view_step_x;
		y = (double)i*grid_view_step_y;

		canvas.drawLine( x, 0, x, client_rect_ht );

		if( i == grid_N/2 )
		{
			pen.setColor( WHITE_COLOR );
			canvas.setPen( pen );
			canvas.drawLine( 0, y, client_rect_wd, y );
			pen.setColor( GRID_COLOR );
			canvas.setPen( pen );
		}
		else
			canvas.drawLine( 0, y, client_rect_wd, y );
	}

	// draw all digits
	pen.setColor( GRID_FONT_COLOR );
	canvas.setPen( pen );
	for( i = 0;i < grid_N;++i )
	{
		x = sx - (double)i*grid_view_step_x;
		y = (double)i*grid_view_step_y;

		if( (val = grid_column - i*(int)grid_step_x) >= 0 )
		{
			str.setNum( val );
			canvas.drawText( x, half_buffer_size_ht + font_ht_k, str );
		}
		str.setNum( (int)(half_vis_range_y - grid_step_y*i) );
		canvas.drawText( 2, y + font_ht_k, str );
	}

}



bool cscroll_graph::add_point( double ra, double dec )
{
	data_idx++;

	if( data_idx == data_cnt-1 )
		data_idx = 0;

	data.line[ RA_LINE ][ data_idx ]  = ra;
	data.line[ DEC_LINE ][ data_idx ] = dec;

	need_refresh = true;

 return true;
}

