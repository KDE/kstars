/***************************************************************************
                          skymapdraw.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Mar 2 2003
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//This file contains drawing functions SkyMap class.

#include <math.h> //log10()

#include "skymap.h"
#include "kstars.h"
#include "infoboxes.h"
#include "indimenu.h"
#include "indidevice.h"

#include <qpaintdevicemetrics.h>
#include <stdlib.h> // abs

void SkyMap::drawBoxes( QPixmap *pm ) {
	if ( ksw ) {
		QPainter p;
		p.begin( pm );
		ksw->infoBoxes()->drawBoxes( p,
				data->options->colorScheme()->colorNamed( "BoxTextColor" ),
				data->options->colorScheme()->colorNamed( "BoxGrabColor" ),
				data->options->colorScheme()->colorNamed( "BoxBGColor" ), false );
		p.end();
	}
}

void SkyMap::drawMilkyWay( QPainter& psky, double scale )
{
	KStarsOptions* options = data->options;

	int ptsCount = 0;
	int mwmax = int( scale * zoomFactor()/100.);
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Milky Way (draw this first so it's in the "background")
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "MWColor" ) ), 1, SolidLine ) ); //change to colorGrid
		psky.setBrush( QBrush( QColor( options->colorScheme()->colorNamed( "MWColor" ) ) ) );
		bool offscreen, lastoffscreen=false;

		for ( register unsigned int j=0; j<11; ++j ) {
			if ( options->fillMilkyWay ) {
				ptsCount = 0;
				bool partVisible = false;

				QPoint o = getXY( data->MilkyWay[j].at(0), options->useAltAz, options->useRefraction, scale );
				if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;

				for ( SkyPoint *p = data->MilkyWay[j].first(); p; p = data->MilkyWay[j].next() ) {
					o = getXY( p, options->useAltAz, options->useRefraction, scale );
					if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;
				}

				if ( ptsCount && partVisible ) {
					psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
	 	  	}
			} else {
	      QPoint o = getXY( data->MilkyWay[j].at(0), options->useAltAz, options->useRefraction, scale );
				if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
				else offscreen = false;

				psky.moveTo( o.x(), o.y() );

				for ( register unsigned int i=1; i<data->MilkyWay[j].count(); ++i ) {
					o = getXY( data->MilkyWay[j].at(i), options->useAltAz, options->useRefraction, scale );
					if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
					else offscreen = false;

					//don't draw a line if the last point's getXY was (-10000000, -10000000)
					int dx = abs(o.x()-psky.pos().x());
					int dy = abs(o.y()-psky.pos().y());
					if ( (!lastoffscreen && !offscreen) && (dx<mwmax && dy<mwmax) ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
					}
					lastoffscreen = offscreen;
				}
			}
		}
	}
}

void SkyMap::drawCoordinateGrid( QPainter& psky, double scale )
{
	KStarsOptions* options = data->options;
	QPoint cur;

	//Draw coordinate grid
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "GridColor" ) ), 1, DotLine ) ); //change to GridColor

		//First, the parallels
		for ( register double Dec=-80.; Dec<=80.; Dec += 20. ) {
			bool newlyVisible = false;
			sp->set( 0.0, Dec );
			if ( options->useAltAz ) sp->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			QPoint o = getXY( sp, options->useAltAz, options->useRefraction, scale );
			QPoint o1 = o;
			cur = o;
			psky.moveTo( o.x(), o.y() );

			double dRA = 1./15.; //180 points along full circle of RA
			for ( register double RA=dRA; RA<24.; RA+=dRA ) {
				sp->set( RA, Dec );
				if ( options->useAltAz ) sp->EquatorialToHorizontal( data->LST, data->geo()->lat() );

				if ( checkVisibility( sp, guideFOV, guideXRange ) ) {
					o = getXY( sp, options->useAltAz, options->useRefraction, scale );

					//When drawing on the printer, the psky.pos() point does NOT get updated
					//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
					int dx = cur.x() - o.x();
					int dy = cur.y() - o.y();
					cur = o;

					if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
						if ( newlyVisible ) {
							newlyVisible = false;
							psky.moveTo( o.x(), o.y() );
						} else {
							psky.lineTo( o.x(), o.y() );
						}
					} else {
						psky.moveTo( o.x(), o.y() );
					}
				}
     }

			//connect the final segment
			int dx = psky.pos().x() - o1.x();
			int dy = psky.pos().y() - o1.y();
			if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
				psky.lineTo( o1.x(), o1.y() );
			} else {
				psky.moveTo( o1.x(), o1.y() );
			}
		}

    //next, the meridians
		for ( register double RA=0.; RA<24.; RA += 2. ) {
			bool newlyVisible = false;
			SkyPoint *sp1 = new SkyPoint( RA, -90. );
			if ( options->useAltAz ) sp1->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			QPoint o = getXY( sp1, options->useAltAz, options->useRefraction, scale );
			cur = o;
			psky.moveTo( o.x(), o.y() );

			double dDec = 1.;
			for ( register double Dec=-89.; Dec<=90.; Dec+=dDec ) {
				sp1->set( RA, Dec );
				if ( options->useAltAz ) sp1->EquatorialToHorizontal( data->LST, data->geo()->lat() );

				if ( checkVisibility( sp1, guideFOV, guideXRange ) ) {
					o = getXY( sp1, options->useAltAz, options->useRefraction, scale );

					//When drawing on the printer, the psky.pos() point does NOT get updated
					//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
					int dx = cur.x() - o.x();
					int dy = cur.y() - o.y();
					cur = o;

					if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
						if ( newlyVisible ) {
							newlyVisible = false;
							psky.moveTo( o.x(), o.y() );
						} else {
							psky.lineTo( o.x(), o.y() );
						}
					} else {
						psky.moveTo( o.x(), o.y() );
					}
				}
			}
			delete sp1;  // avoid memory leak
		}
	}
}

void SkyMap::drawEquator( QPainter& psky, double scale )
{
	KStarsOptions* options = data->options;

	//Draw Equator (currently can't be hidden on slew)
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "EqColor" ) ), 1, SolidLine ) );

		SkyPoint *p = data->Equator.first();
		QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
		QPoint o1 = o;
		QPoint last = o;
		QPoint cur = o;
		psky.moveTo( o.x(), o.y() );
		bool newlyVisible = false;

		//start loop at second item
		for ( p = data->Equator.next(); p; p = data->Equator.next() ) {
			if ( checkVisibility( p, guideFOV, guideXRange ) ) {
				o = getXY( p, options->useAltAz, options->useRefraction, scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;

				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
						if ( newlyVisible ) {
							newlyVisible = false;
							psky.moveTo( last.x(), last.y() );
						}
						psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			} else {
				newlyVisible = true;
			}
			last = o;
		}

		//connect the final segment
		int dx = psky.pos().x() - o1.x();
		int dy = psky.pos().y() - o1.y();
		if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }
}

void SkyMap::drawEcliptic( QPainter& psky, double scale )
{
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Ecliptic (currently can't be hidden on slew)
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "EclColor" ) ), 1, SolidLine ) ); //change to colorGrid

		SkyPoint *p = data->Ecliptic.first();
		QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
		QPoint o1 = o;
		QPoint last = o;
		QPoint cur = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		//Start loop at second item
		for ( p = data->Ecliptic.next(); p; p = data->Ecliptic.next() ) {
			if ( checkVisibility( p, guideFOV, guideXRange ) ) {
				o = getXY( p, options->useAltAz, options->useRefraction, scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;

				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( last.x(), last.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			} else {
				newlyVisible = true;
			}
			last = o;
		}

		//connect the final segment
		int dx = psky.pos().x() - o1.x();
		int dy = psky.pos().y() - o1.y();
		if ( abs(dx) < Width && abs(dy) < Height ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }
}

void SkyMap::drawConstellationLines( QPainter& psky, double scale )
{
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Constellation Lines
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "CLineColor" ) ), 1, SolidLine ) ); //change to colorGrid
		int iLast = -1;

		for ( SkyPoint *p = data->clineList.first(); p; p = data->clineList.next() ) {
			QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );

			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
				if ( data->clineModeList.at(data->clineList.at())->latin1()=='M' ) {
					psky.moveTo( o.x(), o.y() );
				} else if ( data->clineModeList.at(data->clineList.at())->latin1()=='D' ) {
					if ( data->clineList.at()== (int)(iLast+1) ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
  	      }
				}
				iLast = data->clineList.at();
			}
		}
  }
}

void SkyMap::drawConstellationNames( QPainter& psky, QFont& stdFont, double scale ) {
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Constellation Names
	//Don't draw names if slewing
	if ( true ) {
		psky.setFont( stdFont );
		psky.setPen( QColor( options->colorScheme()->colorNamed( "CNameColor" ) ) );
		for ( SkyObject *p = data->cnameList.first(); p; p = data->cnameList.next() ) {
			if ( checkVisibility( p, fov(), XRange ) ) {
				QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
				if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
					if ( options->useLatinConstellNames ) {
						int dx = 5*p->name().length();
						psky.drawText( o.x()-dx, o.y(), p->name() );  // latin constellation names
					} else if ( options->useLocalConstellNames ) {
						// can't use translatedName() because we need the context string in i18n()
						int dx = 5*( i18n( "Constellation name (optional)", p->name().local8Bit().data() ).length() );
						psky.drawText( o.x()-dx, o.y(), i18n( "Constellation name (optional)", p->name().local8Bit().data() ) ); // localized constellation names
					} else {
						int dx = 5*p->name2().length();
						psky.drawText( o.x()-dx, o.y(), p->name2() ); //name2 is the IAU abbreviation
					}
				}
			}
		}
  }
}

void SkyMap::drawStars( QPainter& psky, double scale ) {
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	bool checkSlewing = ( ( slewing || ( clockSlewing && data->clock()->isActive() ) )
				&& options->hideOnSlew );

//shortcuts to inform wheter to draw different objects
	bool hideFaintStars( checkSlewing && options->hideStars );

	if ( options->drawSAO ) {
		float maglim = options->magLimitDrawStar;

		//adjust maglimit for ZoomLevel
		double lgmin = log10(MINZOOM);
		double lgmax = log10(MAXZOOM);
		double lgz = log10(zoomFactor());
		if ( lgz < 0.75*lgmax ) {
			maglim -= 2.0*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
		}

		for ( StarObject *curStar = data->starList.first(); curStar; curStar = data->starList.next() ) {
			// break loop if maglim is reached
			if ( curStar->mag() > maglim || ( hideFaintStars && curStar->mag() > options->magLimitHideStar ) ) break;

			if ( checkVisibility( curStar, fov(), XRange ) ) {
				QPoint o = getXY( curStar, options->useAltAz, options->useRefraction, scale );

				// draw star if currently on screen
				if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
					float sizeFactor = 1.0 + 1.9*( lgz - lgmin )/(lgmax - lgmin);
					int size = int( sizeFactor*( maglim - curStar->mag()) ) + 1;

					if ( size > 0 ) {
						psky.setPen( QColor( options->colorScheme()->colorNamed( "SkyColor" ) ) );
						drawSymbol( psky, curStar->type(), o.x(), o.y(), size, 1.0, 0, curStar->color(), scale );

						// now that we have drawn the star, we can display some extra info
						if ( !checkSlewing && (curStar->mag() <= options->magLimitDrawStarInfo )
								&& ( ( options->drawStarName && curStar->name() != "star" )
										|| options->drawStarMagnitude ) ) {

							psky.setPen( QColor( options->colorScheme()->colorNamed( "SNameColor" ) ) );
							curStar->drawLabel( psky, o.x(), o.y(), zoomFactor(),
									options->drawStarName, options->drawStarMagnitude, scale );
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawDeepSkyCatalog( QPainter& psky, QPtrList<DeepSkyObject>& catalog, QColor& color,
			bool drawObject, bool drawImage, double scale )
{
	KStarsOptions* options = data->options;
	QImage ScaledImage;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	// Set color once
	psky.setPen( color );
	psky.setBrush( NoBrush );
	QColor colorHST  = options->colorScheme()->colorNamed( "HSTColor" );

	//Draw Deep-Sky Objects
	for ( DeepSkyObject *obj = catalog.first(); obj; obj = catalog.next() ) {

		if ( drawObject || drawImage ) {
			if ( checkVisibility( obj, fov(), XRange ) ) {
				QPoint o = getXY( obj, options->useAltAz, options->useRefraction, scale );
				if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
					int PositionAngle = findPA( obj, o.x(), o.y() );

					//Draw Image
					if ( drawImage && zoomFactor() > 5.*MINZOOM ) {
						QFile file;

						//readImage reads image from disk, or returns pointer to existing image.
						QImage *image=obj->readImage();
						if ( image ) {
							int w = int( obj->a() * scale * dms::PI * zoomFactor()/10800.0 );
							int h = int( w*image->height()/image->width() ); //preserve image's aspect ratio
							int dx = int( 0.5*w );
							int dy = int( 0.5*h );
							ScaledImage = image->smoothScale( w, h );
							psky.save();
							psky.translate( o.x(), o.y() );
							psky.rotate( double( PositionAngle ) );  //rotate the coordinate system
							psky.drawImage( -dx, -dy, ScaledImage );
							psky.restore();
						}
					}

					//Draw Symbol
					if ( drawObject ) {
						//change color if extra images are available
            // most objects don't have those, so we only change colors temporarily
            // for the few exceptions. Changing color is expensive!!!
            bool bColorChanged = false;
						if ( obj->isCatalogM() && obj->ImageList.count() > 1 ) {
							psky.setPen( colorHST );
              bColorChanged = true;
            }
						else if ( (!obj->isCatalogM()) && obj->ImageList.count() ) {
							psky.setPen( colorHST );
							bColorChanged = true;
            }

						float majorAxis = obj->a();
						// if size is 0.0 set it to 1.0, this are normally stars (type 0 and 1)
						// if we use size 0.0 the star wouldn't be drawn
						if ( majorAxis == 0.0 && obj->type() < 2 ) {
							majorAxis = 1.0;
						}

						//scale parameter is included in drawSymbol, so we don't place it here in definition of Size
						int Size = int( majorAxis * dms::PI * zoomFactor()/10800.0 );

						// use star draw function
						drawSymbol( psky, obj->type(), o.x(), o.y(), Size, obj->e(), PositionAngle, 0, scale );
            // revert temporary color change
            if ( bColorChanged ) {
							psky.setPen( color );
            }
					}
				}
			} else { //Object failed checkVisible(); delete it's Image pointer, if it exists.
				if ( obj->image() ) {
					obj->deleteImage();
				}
			}
		}
	}

}

void SkyMap::drawDeepSkyObjects( QPainter& psky, double scale )
{
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	QImage ScaledImage;

	bool checkSlewing = ( ( slewing || ( clockSlewing && data->clock()->isActive() ) )
				&& options->hideOnSlew );

//shortcuts to inform wheter to draw different objects
	bool drawMess( options->drawDeepSky && ( options->drawMessier || options->drawMessImages ) && !(checkSlewing && options->hideMess) );
	bool drawNGC( options->drawDeepSky && options->drawNGC && !(checkSlewing && options->hideNGC) );
	bool drawOther( options->drawDeepSky && options->drawOther && !(checkSlewing && options->hideOther) );
	bool drawIC( options->drawDeepSky && options->drawIC && !(checkSlewing && options->hideIC) );

	// calculate color objects once, outside the loop
	QColor colorMess = options->colorScheme()->colorNamed( "MessColor" );
	QColor colorIC  = options->colorScheme()->colorNamed( "ICColor" );
	QColor colorNGC  = options->colorScheme()->colorNamed( "NGCColor" );

	// draw Messier catalog
	if ( drawMess ) {
		bool drawObject = options->drawMessier;
		bool drawImage = options->drawMessImages;
		drawDeepSkyCatalog( psky, data->deepSkyListMessier, colorMess, drawObject, drawImage, scale );
	}

	// draw NGC Catalog
	if ( drawNGC ) {
		bool drawObject = true;
		bool drawImage = options->drawMessImages;
		drawDeepSkyCatalog( psky, data->deepSkyListNGC, colorNGC, drawObject, drawImage, scale );
	}

	// draw IC catalog
	if ( drawIC ) {
		bool drawObject = true;
		bool drawImage = options->drawMessImages;
	drawDeepSkyCatalog( psky, data->deepSkyListIC, colorIC, drawObject, drawImage, scale );
	}

	// draw the rest
	if ( drawOther ) {
		bool drawObject = true;
		bool drawImage = options->drawMessImages;
		//Use NGC color for now...
		drawDeepSkyCatalog( psky, data->deepSkyListOther, colorNGC, drawObject, drawImage, scale );
	}

	//Draw Custom Catalogs
	for ( register unsigned int i=0; i<options->CatalogCount; ++i ) { //loop over custom catalogs
		if ( options->drawCatalog[i] ) {

			psky.setBrush( NoBrush );
			psky.setPen( QColor( options->colorScheme()->colorNamed( "NGCColor" ) ) );

			QPtrList<DeepSkyObject> cat = data->CustomCatalogs[ options->CatalogName[i] ];

			for ( DeepSkyObject *obj = cat.first(); obj; obj = cat.next() ) {

				if ( checkVisibility( obj, fov(), XRange ) ) {
					QPoint o = getXY( obj, options->useAltAz, options->useRefraction, scale );

					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {

						if ( obj->type()==0 || obj->type()==1 ) {
							StarObject *starobj = (StarObject*)obj;
//							float zoomlim = 7.0 + float( options->ZoomLevel )/4.0;
							float zoomlim = 7.0 + (zoomFactor()/MINZOOM)/50.0;

							float mag = starobj->mag();
							float sizeFactor = 2.0;
							int size = int( sizeFactor*(zoomlim - mag) ) + 1;
							if (size>23) size=23;
							if ( size > 0 ) drawSymbol( psky, starobj->type(), o.x(), o.y(), size, 1.0, 0, starobj->color(), scale );
						} else {
							int size = int(zoomFactor()/MINZOOM);
							if (size>8) size = 8;
							drawSymbol( psky, obj->type(), o.x(), o.y(), size, 1.0, 0, 0, scale );
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawAttachedLabels( QPainter &psky, double scale ) {
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	psky.setPen( data->options->colorScheme()->colorNamed( "UserLabelColor" ) );

	bool checkSlewing = ( slewing || ( clockSlewing && data->clock()->isActive() ) ) && data->options->hideOnSlew;
	bool drawPlanets( data->options->drawPlanets && !(checkSlewing && data->options->hidePlanets) );
	bool drawComets( drawPlanets && data->options->drawComets );
	bool drawAsteroids( drawPlanets && data->options->drawAsteroids );
	bool drawMessier( data->options->drawDeepSky && ( data->options->drawMessier || data->options->drawMessImages ) && !(checkSlewing && data->options->hideMess) );
	bool drawNGC( data->options->drawDeepSky && data->options->drawNGC && !(checkSlewing && data->options->hideNGC) );
	bool drawIC( data->options->drawDeepSky && data->options->drawIC && !(checkSlewing && data->options->hideIC) );
	bool drawOther( data->options->drawDeepSky && data->options->drawOther && !(checkSlewing && data->options->hideOther) );
	bool drawSAO = ( data->options->drawSAO );
	bool hideFaintStars( checkSlewing && data->options->hideStars );

	for ( SkyObject *obj = data->ObjLabelList.first(); obj; obj = data->ObjLabelList.next() ) {
		//Only draw an attached label if the object is being drawn to the map
		//reproducing logic from other draw funcs here...not an optimal solution
		if ( obj->type() == SkyObject::STAR ) {
			if ( ! drawSAO ) return;
			if ( obj->mag() > data->options->magLimitDrawStar ) return;
			if ( hideFaintStars && obj->mag() > data->options->magLimitHideStar ) return;
		}
		if ( obj->type() == SkyObject::PLANET ) {
			if ( ! drawPlanets ) return;
			if ( obj->name() == "Sun" && ! data->options->drawSun ) return;
			if ( obj->name() == "Mercury" && ! data->options->drawMercury ) return;
			if ( obj->name() == "Venus" && ! data->options->drawVenus ) return;
			if ( obj->name() == "Moon" && ! data->options->drawMoon ) return;
			if ( obj->name() == "Mars" && ! data->options->drawMars ) return;
			if ( obj->name() == "Jupiter" && ! data->options->drawJupiter ) return;
			if ( obj->name() == "Saturn" && ! data->options->drawSaturn ) return;
			if ( obj->name() == "Uranus" && ! data->options->drawUranus ) return;
			if ( obj->name() == "Neptune" && ! data->options->drawNeptune ) return;
			if ( obj->name() == "Pluto" && ! data->options->drawPluto ) return;
		}
		if ( obj->type() >= SkyObject::OPEN_CLUSTER && obj->type() <= SkyObject::GALAXY ) {
			if ( ((DeepSkyObject*)obj)->isCatalogM() && ! drawMessier ) return;
			if ( ((DeepSkyObject*)obj)->isCatalogNGC() && ! drawNGC ) return;
			if ( ((DeepSkyObject*)obj)->isCatalogIC() && ! drawIC ) return;
			if ( ((DeepSkyObject*)obj)->isCatalogNone() && ! drawOther ) return;
		}
		if ( obj->type() == SkyObject::COMET && ! drawComets ) return;
		if ( obj->type() == SkyObject::ASTEROID && ! drawAsteroids ) return;

		if ( checkVisibility( obj, fov(), XRange ) ) {
			QPoint o = getXY( obj, data->options->useAltAz, data->options->useRefraction, scale );
			if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
				drawNameLabel( psky, obj, o.x(), o.y(), scale );
			}
		}
	}

	//Attach a label to the centered object
	if ( focusObject() != NULL && data->options->useAutoLabel ) {
		QPoint o = getXY( focusObject(), data->options->useAltAz, data->options->useRefraction, scale );
		if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height )
			drawNameLabel( psky, focusObject(), o.x(), o.y(), scale );
	}
}

void SkyMap::drawNameLabel( QPainter &psky, SkyObject *obj, int x, int y, double scale ) {
	int size(0);

	//Stars
	if ( obj->type() == SkyObject::STAR ) {
		((StarObject*)obj)->drawLabel( psky, x, y, zoomFactor(), true, false, scale );
		return;

	//Solar system
	} else if ( obj->type() == SkyObject::PLANET
						|| obj->type() == SkyObject::ASTEROID
						|| obj->type() == SkyObject::COMET ) {
		KSPlanetBase *p = (KSPlanetBase*)obj;
		size = int( p->angSize() * scale * dms::PI * zoomFactor()/10800.0 );
		int minsize = 4;
		if ( p->type() == SkyObject::ASTEROID || p->type() == SkyObject::COMET )
			minsize = 2;
		if ( p->name() == "Sun" || p->name() == "Moon" )
			minsize = 8;
		if ( size < minsize )
			size = minsize;

	//Other
	} else {
		//Calculate object size in pixels
		float majorAxis = ((DeepSkyObject*)obj)->a();
		if ( majorAxis == 0.0 && obj->type() == 1 ) majorAxis = 1.0; //catalog stars
		size = int( majorAxis * scale * dms::PI * zoomFactor()/10800.0 );
	}

	int offset = int( ( 0.5*size + 4 ) );
	psky.drawText( x+offset, y+offset, i18n(obj->name().local8Bit()) );
}

void SkyMap::drawPlanetTrail( QPainter& psky, KSPlanetBase *ksp, double scale )
{
	if ( ksp->hasTrail() ) {
		KStarsOptions* options = data->options;

		int Width = int( scale * width() );
		int Height = int( scale * height() );

		QColor tcolor1 = QColor( options->colorScheme()->colorNamed( "PlanetTrailColor" ) );
		QColor tcolor2 = QColor( options->colorScheme()->colorNamed( "SkyColor" ) );

		SkyPoint *p = ksp->trail()->first();
		QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
		bool firstPoint(false);
		int i = 0;
		int n = ksp->trail()->count();

		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			psky.moveTo(o.x(), o.y());
			firstPoint = true;
		}

		psky.setPen( QPen( tcolor1, 1 ) );
		for ( p = ksp->trail()->next(); p; p = ksp->trail()->next() ) {
			if ( options->fadePlanetTrails ) {
				//Define interpolated color
				QColor tcolor = QColor(
							(i*tcolor1.red()   + (n-i)*tcolor2.red())/n,
							(i*tcolor1.green() + (n-i)*tcolor2.green())/n,
							(i*tcolor1.blue()  + (n-i)*tcolor2.blue())/n );
				++i;
				psky.setPen( QPen( tcolor, 1 ) );
			}

			o = getXY( p, options->useAltAz, options->useRefraction, scale );
			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

				if ( firstPoint ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
					firstPoint = true;
				}
			}
		}
	}
}

void SkyMap::drawSolarSystem( QPainter& psky, bool drawPlanets, double scale )
{
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	if (drawPlanets == true) {
		//Draw Sun
		if ( options->drawSun ) {
			drawPlanet(psky, data->PC->findByName("Sun"), QColor( "Yellow" ), MINZOOM, 1, scale );
		}

		//Draw Moon
		if ( options->drawMoon ) {
			drawPlanet(psky, data->Moon, QColor( "White" ), MINZOOM, 1, scale );
		}

		//Draw Mercury
		if ( options->drawMercury ) {
			drawPlanet(psky, data->PC->findByName("Mercury"), QColor( "SlateBlue1" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Venus
		if ( options->drawVenus ) {
			drawPlanet(psky, data->PC->findByName("Venus"), QColor( "LightGreen" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Mars
		if ( options->drawMars ) {
			drawPlanet(psky, data->PC->findByName("Mars"), QColor( "Red" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Jupiter
		if ( options->drawJupiter ) {
			drawPlanet(psky, data->PC->findByName("Jupiter"), QColor( "Goldenrod" ), 20.*MINZOOM, 1, scale );

			//Draw Jovian moons
			psky.setPen( QPen( QColor( "white" ) ) );
			if ( zoomFactor() > 10.*MINZOOM ) {
				QFont pfont = psky.font();
				QFont moonFont = psky.font();
				moonFont.setPointSize( pfont.pointSize() - 2 );
				psky.setFont( moonFont );

				for ( unsigned int i=0; i<4; ++i ) {
					QPoint o = getXY( data->jmoons->pos(i), options->useAltAz, options->useRefraction, scale );
					if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
						psky.drawEllipse( o.x()-1, o.y()-1, 2, 2 );

						//Draw Moon name labels if at high zoom
						if ( options->drawPlanetName && zoomFactor() > 50.*MINZOOM ) {
							int offset = int(3*scale);
							psky.drawText( o.x() + offset, o.y() + offset, data->jmoons->name(i) );
						}
					}
				}

				//reset font
				psky.setFont( pfont );
			}
		}

		//Draw Saturn
		if ( options->drawSaturn ) {
			drawPlanet(psky, data->PC->findByName("Saturn"), QColor( "LightYellow2" ), 20.*MINZOOM, 2, scale );
		}

		//Draw Uranus
		if ( options->drawUranus && drawPlanets ) {
			drawPlanet(psky, data->PC->findByName("Uranus"), QColor( "LightSeaGreen" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Neptune
		if ( options->drawNeptune ) {
			drawPlanet(psky, data->PC->findByName("Neptune"), QColor( "SkyBlue" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Pluto
		if ( options->drawPluto ) {
			drawPlanet(psky, data->PC->findByName("Pluto"), QColor( "gray" ), 50.*MINZOOM, 1, scale );
		}

		//Draw Asteroids
		if ( options->drawAsteroids ) {
			for ( KSAsteroid *ast = data->asteroidList.first(); ast; ast = data->asteroidList.next() ) {
				if ( ast->mag() > data->options->magLimitAsteroid ) break;

				//draw Trail
				if ( ast->hasTrail() ) drawPlanetTrail( psky, ast, scale );

				psky.setPen( QPen( QColor( "gray" ) ) );
				psky.setBrush( QBrush( QColor( "gray" ) ) );
				QPoint o = getXY( ast, options->useAltAz, options->useRefraction, scale );

				if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
					int size = int( ast->angSize() * scale * dms::PI * zoomFactor()/10800.0 );
					if ( size < 2 ) size = 2;
					int x1 = o.x() - size/2;
					int y1 = o.y() - size/2;
					psky.drawEllipse( x1, y1, size, size );

					//draw Name
					if ( data->options->drawAsteroidName && ast->mag() < data->options->magLimitAsteroidName ) {
						psky.setPen( QColor( data->options->colorScheme()->colorNamed( "PNameColor" ) ) );
						drawNameLabel( psky, ast, o.x(), o.y(), scale );
					}
				}
			}
		}

		//Draw Comets
		if ( options->drawComets ) {
			for ( KSComet *com = data->cometList.first(); com; com = data->cometList.next() ) {

				//draw Trail
				if ( com->hasTrail() ) drawPlanetTrail( psky, com, scale );

				psky.setPen( QPen( QColor( "cyan4" ) ) );
				psky.setBrush( QBrush( QColor( "cyan4" ) ) );
				if ( zoomFactor() > 10.*MINZOOM ) {
					QPoint o = getXY( com, options->useAltAz, options->useRefraction, scale );

					if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
						int size = int( com->angSize() * scale * dms::PI * zoomFactor()/10800.0 );
						if ( size < 2 ) size = 2;
						int x1 = o.x() - size/2;
						int y1 = o.y() - size/2;
						psky.drawEllipse( x1, y1, size, size );

						//draw Name
						if ( data->options->drawCometName && com->rsun() < data->options->maxRadCometName ) {
							psky.setPen( QColor( data->options->colorScheme()->colorNamed( "PNameColor" ) ) );
							drawNameLabel( psky, com, o.x(), o.y(), scale );
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawPlanet( QPainter &psky, KSPlanetBase *p, QColor c,
		double zoommin, int resize_mult, double scale ) {

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//draw Trail
	if ( p->hasTrail() ) drawPlanetTrail( psky, p, scale );

	int sizemin = 4;
	if ( p->name() == "Sun" || p->name() == "Moon" ) sizemin = 8;
	sizemin = int( sizemin * scale );

	psky.setPen( c );
	psky.setBrush( c );
	QPoint o = getXY( p, data->options->useAltAz, data->options->useRefraction, scale );

	if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
		//image angle is PA plus the North angle.
		//Find North angle:
		SkyPoint test;
		test.set( p->ra()->Hours(), p->dec()->Degrees() + 1.0 );
		if ( data->options->useAltAz ) test.EquatorialToHorizontal( data->LST, data->geo()->lat() );
		QPoint t = getXY( &test, data->options->useAltAz, data->options->useRefraction, scale );
		double dx = double( o.x() - t.x() );  //backwards to get counterclockwise angle
		double dy = double( t.y() - o.y() );
		double north(0.0);
		if ( dy ) {
			north = atan( dx/dy )*180.0/dms::PI;
		} else {
			north = 90.0;
			if ( dx > 0 ) north = -90.0;
		}

		//Image size must be modified to account for possibility that rotated image's
		//size is bigger than original image size.  The rotated image is a square
		//superscribed on the original image.  The superscribed square is larger than
		//the original square by a factor of (cos(t) + sin(t)) where t is the angle by
		//which the two squares are rotated (in our case, equal to the position angle +
		//the north angle, reduced between 0 and 90 degrees).
		//The proof is left as an exercise to the student :)
		dms pa( p->pa() + north );
		double spa, cpa;
		pa.SinCos( spa, cpa );
		cpa = fabs(cpa); spa = fabs(spa);

		int size = int( p->angSize() * scale * dms::PI * zoomFactor()/10800.0 * (cpa + spa) );
		//int size = int( p->angSize() * scale * dms::PI * zoomFactor()/10800.0 );
		if ( size < sizemin ) size = sizemin;
                                               //Only draw planet image if:
		if ( data->options->drawPlanetImage &&     //user wants them,
				int(zoomFactor()) >= int(zoommin) &&   //zoomed in enough,
				!p->image()->isNull() &&               //image loaded ok,
				size < Width ) {                       //and size isn't too big.

			if (resize_mult != 1) {
				size *= resize_mult;
			}

			p->scaleRotateImage( size, p->pa() + north );
			int x1 = o.x() - p->image()->width()/2;
			int y1 = o.y() - p->image()->height()/2;
			psky.drawImage( x1, y1, *(p->image()));

		} else {                                   //Otherwise, draw a simple circle.

			psky.drawEllipse( o.x()-size/2, o.y()-size/2, size, size );
		}

		//draw Name
		if ( data->options->drawPlanetName ) {
			psky.setPen( QColor( data->options->colorScheme()->colorNamed( "PNameColor" ) ) );
			drawNameLabel( psky, p, o.x(), o.y(), scale );
		}
	}
}

void SkyMap::drawHorizon( QPainter& psky, QFont& stdFont, double scale )
{
	KStarsOptions* options = data->options;

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	QPtrList<QPoint> points;
	points.setAutoDelete(true);
	//Draw Horizon
	//The horizon should not be corrected for atmospheric refraction, so getXY has doRefract=false...
	if (options->drawHorizon || options->drawGround) {
		QPoint OutLeft(0,0), OutRight(0,0);

		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "HorzColor" ) ), 1, SolidLine ) ); //change to colorGrid
		psky.setBrush( QColor ( options->colorScheme()->colorNamed( "HorzColor" ) ) );
		int ptsCount = 0;

		int maxdist = int(zoomFactor()/4);

		for ( SkyPoint *p = data->Horizon.first(); p; p = data->Horizon.next() ) {
			QPoint *o = new QPoint();
			*o = getXY( p, options->useAltAz, false, scale );  //false: do not refract the horizon
			bool found = false;

			//Use the QPtrList of points to pre-sort visible horizon points
//			if ( o->x() > -1*maxdist && o->x() < width() + maxdist ) {
			if ( o->x() > -100 && o->x() < Width + 100 && o->y() > -100 && o->y() < Height + 100 ) {
				if ( options->useAltAz ) {
					register unsigned int j;
					for ( j=0; j<points.count(); ++j ) {
						if ( o->x() < points.at(j)->x() ) {
							found = true;
							break;
						}
					}
					if ( found ) {
						points.insert( j, o );
					} else {
						points.append( o );
					}
				} else {
					points.append( o );
				}
			} else {  //find the out-of-bounds points closest to the left and right borders
				if ( ( OutLeft.x() == 0 || o->x() > OutLeft.x() ) && o->x() < -100 ) {
					OutLeft.setX( o->x() );
					OutLeft.setY( o->y() );
				}
				if ( ( OutRight.x() == 0 || o->x() < OutRight.x() ) && o->x() >  + 100 ) {
					OutRight.setX( o->x() );
					OutRight.setY( o->y() );
				}
				// delete non stored points to avoid memory leak
				delete o;
			}
		}

		//Add left-edge and right-edge points based on interpolating the first/last onscreen points
		//to the nearest offscreen points.
		if ( options->useAltAz && points.count() > 0 ) {
     //Interpolate from first sorted onscreen point to x=-100,
     //using OutLeft to determine the slope
			int xtotal = ( points.at( 0 )->x() - OutLeft.x() );
			int xx = ( points.at( 0 )->x() + 100 ) / xtotal;
			int yp = xx*OutRight.y() + (1-xx)*points.at( 0 )->y();  //interpolated left-edge y value
			QPoint *LeftEdge = new QPoint( -100, yp );
			points.insert( 0, LeftEdge ); //Prepend LeftEdge to the beginning of points

    	//Interpolate from the last sorted onscreen point to ()+100,
			//using OutRight to determine the slope.
			xtotal = ( OutRight.x() - points.at( points.count() - 1 )->x() );
			xx = ( Width + 100 - points.at( points.count() - 1 )->x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*points.at( points.count() - 1 )->y(); //interpolated right-edge y value
			QPoint *RightEdge = new QPoint( Width+100, yp );
			points.append( RightEdge );

//If there are no horizon points, then either the horizon doesn't pass through the screen
//or we're at high zoom, and horizon points lie on either side of the screen.
		} else if ( options->useAltAz && OutLeft.y() !=0 && OutRight.y() !=0 &&
            !( OutLeft.y() > Height + 100 && OutRight.y() > Height + 100 ) &&
            !( OutLeft.y() < -100 && OutRight.y() < -100 ) ) {

     //It's possible at high zoom that /no/ horizon points are onscreen.  In this case,
     //interpolate between OutLeft and OutRight directly to construct the horizon polygon.
			int xtotal = ( OutRight.x() - OutLeft.x() );
			int xx = ( OutRight.x() + 100 ) / xtotal;
			int yp = xx*OutLeft.y() + (1-xx)*OutRight.y();  //interpolated left-edge y value
			QPoint *LeftEdge = new QPoint( -100, yp );
			points.append( LeftEdge );

			xx = ( Width + 100 - OutLeft.x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*OutLeft.y(); //interpolated right-edge y value
			QPoint *RightEdge = new QPoint( Width+100, yp );
			points.append( RightEdge );
 		}

		if ( points.count() ) {
//		Fill the pts array with sorted horizon points, Draw Horizon Line
			pts->setPoint( 0, points.at(0)->x(), points.at(0)->y() );
			if ( options->drawHorizon ) psky.moveTo( points.at(0)->x(), points.at(0)->y() );

			for ( register unsigned int i=1; i<points.count(); ++i ) {
				pts->setPoint( i, points.at(i)->x(), points.at(i)->y() );

				if ( options->drawHorizon ) {
					if ( !options->useAltAz && ( abs( points.at(i)->x() - psky.pos().x() ) > maxdist ||
								abs( points.at(i)->y() - psky.pos().y() ) > maxdist ) ) {
						psky.moveTo( points.at(i)->x(), points.at(i)->y() );
					} else {
						psky.lineTo( points.at(i)->x(), points.at(i)->y() );
					}

				}
			}

  	 //connect the last segment back to the beginning
			if ( abs( points.at(0)->x() - psky.pos().x() ) < maxdist && abs( points.at(0)->y() - psky.pos().y() ) < maxdist )
				psky.lineTo( points.at(0)->x(), points.at(0)->y() );

//DUMP_HORIZON
/*
			if (dumpHorizon) {
				//First, make sure output file doesn't exist yet (so this only happens once)
				QFile dumpFile( "horizon.xy" );
				if ( !dumpFile.exists() && dumpFile.open( IO_WriteOnly ) ) {
					QTextStream t( &dumpFile );
					for ( register uint i=0; i < points.count(); ++i ) {
						t << points.at(i)->x() << " " << points.at(i)->y() << endl;
					}
					dumpFile.close();
				}

				dumpHorizon = false;
			}
*/
//END_DUMP_HORIZON

//		Finish the Ground polygon by adding a square bottom edge, offscreen
			if ( options->useAltAz && options->drawGround ) {
				ptsCount = points.count();
				pts->setPoint( ptsCount++, Width+100, Height+100 );   //bottom right corner
				pts->setPoint( ptsCount++, -100, Height+100 );         //bottom left corner

				psky.drawPolygon( ( const QPointArray ) *pts, false, 0, ptsCount );

//  remove all items in points list
				for ( register unsigned int i=0; i<points.count(); ++i ) {
					points.remove(i);
				}

//	Draw compass heading labels along horizon
				SkyPoint *c = new SkyPoint;
				QPoint cpoint;
				psky.setPen( QColor ( options->colorScheme()->colorNamed( "CompassColor" ) ) );
				psky.setFont( stdFont );

		//North
				c->setAz( 359.99 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "North", "N" ) );
				}

		//NorthEast
				c->setAz( 45.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northeast", "NE" ) );
				}

		//East
				c->setAz( 90.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "East", "E" ) );
				}

		//SouthEast
				c->setAz( 135.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southeast", "SE" ) );
				}

		//South
				c->setAz( 179.99 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "South", "S" ) );
				}

		//SouthWest
				c->setAz( 225.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southwest", "SW" ) );
				}

		//West
				c->setAz( 270.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "West", "W" ) );
				}

		//NorthWest
				c->setAz( 315.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northwest", "NW" ) );
				}

				delete c;
			}
		}
	}  //endif drawing horizon
}

void SkyMap::drawTelescopeSymbols(QPainter &psky)
{

  INDI_P *eqNum;
  INDI_L *lp;
  INDIMenu *devMenu = ksw->getINDIMenu();
  KStarsOptions* options = ksw->options();

  if (!options->indiCrosshairs || devMenu == NULL)
   return;

  psky.setPen( QPen( QColor( data->options->colorScheme()->colorNamed("TargetColor" ) ) ) );
	psky.setBrush( NoBrush );
	int pxperdegree = int(zoomFactor()/57.3);

  //fprintf(stderr, "in draw telescope function with mgrsize of %d\n", devMenu->mgr.size());
  for (uint i=0; i < devMenu->mgr.size(); i++)
    for (uint j=0; j < devMenu->mgr[i]->indi_dev.size(); j++)
    {
      // make sure the dev is on first
      if (devMenu->mgr[i]->indi_dev[j]->isOn())
      {
        eqNum = devMenu->mgr[i]->indi_dev[j]->findProp(QString("EQUATORIAL_COORD"));

	//fprintf(stderr, "Looking for EQUATORIAL_COORD prop\n");
        // make sure it has RA and DEC properties
        if ( eqNum)
	{

	//fprintf(stderr, "Looking for RA label\n");
	lp = eqNum->findLabel(QString("RA"));
	if (!lp)
	 continue;

	 dms raDMS(lp->value);

	 //fprintf(stderr, "RA (%s) value is %g\n", lp->text, lp->value);

	 //fprintf(stderr, "Looking for DEC label\n");
	lp = eqNum->findLabel(QString("DEC"));
	if (!lp)
	 continue;

	dms decDMS(lp->value);

	//fprintf(stderr, "DEC (%s) value is %g\n", lp->text, lp->value);

	 raDMS.setD ( raDMS.Degrees() * 15.0);

	 //fprintf(stderr, "RA is %d , DEC is %g\n", raDMS.Degrees(), decDMS.Degrees());

	 //kdDebug() << "the KStars RA is " << raDMS->toHMSString() << endl;
	 //kdDebug() << "the KStars DEC is " << decDMS->toDMSString() << "\n****************" << endl;

	 SkyPoint sp( &raDMS, &decDMS);
	 sp.apparentCoord( (double) J2000, ksw->data()->clock()->JD());
	 sp.EquatorialToHorizontal( ksw->LST(), ksw->geo()->lat() );

	 QPoint P = getXY( &sp, options->useAltAz, options->useRefraction );

	int s1 = pxperdegree/2;
	int s2 = pxperdegree;
	int s3 = 2*pxperdegree;

	int x0 = P.x();  int y0 = P.y();
	int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
	int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
	int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

	//Draw radial lines
	psky.drawLine( x1, y0, x3, y0 );
	psky.drawLine( x0+s2, y0, x0+s1/2, y0 );
	psky.drawLine( x0, y1, x0, y3 );
	psky.drawLine( x0, y0+s1/2, x0, y0+s2 );
	//Draw circles at 0.5 & 1 degrees
	psky.drawEllipse( x1, y1, s1, s1 );
	psky.drawEllipse( x2, y2, s2, s2 );

	//FIXME the label is not displayed optimally (needs to be somewhat centered vertically)
	psky.drawText( x0+s2 + 2 , y0, QString(devMenu->mgr[i]->indi_dev[j]->name) );


	}
      }
    }

}

void SkyMap::drawTargetSymbol( QPainter &psky, int style ) {
	//Draw this last so it is never "behind" other things.
	psky.setPen( QPen( QColor( data->options->colorScheme()->colorNamed("TargetColor" ) ) ) );
	psky.setBrush( NoBrush );
	int pxperdegree = int(zoomFactor()/57.3);

	switch ( style ) {
		case 1: { //simple circle, one degree in diameter.
			int size = pxperdegree;
			psky.drawEllipse( width()/2-size/2, height()/2-size/2, size, size );
			break;
		}
		case 2: { //case 1, fancy crosshairs
			int s1 = pxperdegree/2;
			int s2 = pxperdegree;
			int s3 = 2*pxperdegree;

			int x0 = width()/2;  int y0 = height()/2;
			int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
			int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
			int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

			//Draw radial lines
			psky.drawLine( x1, y0, x3, y0 );
			psky.drawLine( x0+s2, y0, x0+s1/2, y0 );
			psky.drawLine( x0, y1, x0, y3 );
			psky.drawLine( x0, y0+s1/2, x0, y0+s2 );

			//Draw circles at 0.5 & 1 degrees
			psky.drawEllipse( x1, y1, s1, s1 );
			psky.drawEllipse( x2, y2, s2, s2 );

			break;
		}

		case 3: { //Bullseye
			int s1 = pxperdegree/2;
			int s2 = pxperdegree;
			int s3 = 2*pxperdegree;

			int x0 = width()/2;  int y0 = height()/2;
			int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
			int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
			int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

			psky.drawEllipse( x1, y1, s1, s1 );
			psky.drawEllipse( x2, y2, s2, s2 );
			psky.drawEllipse( x3, y3, s3, s3 );

			break;
		}

		case 4: { //Rectangle
			int s = pxperdegree;
			int x1 = width()/2 - s/2;
			int y1 = height()/2 - s/2;

			psky.drawRect( x1, y1, s, s );
			break;
		}

	}
}

void SkyMap::drawSymbol( QPainter &psky, int type, int x, int y, int size, double e, int pa, QChar color, double scale ) {
	size = int( scale * size );

	int dx1 = -size/2;
	int dx2 =  size/2;
	int dy1 = int( -e*size/2 );
	int dy2 = int( e*size/2 );
	int x1 = x + dx1;
	int x2 = x + dx2;
	int y1 = y + dy1;
	int y2 = y + dy2;

	int dxa = -size/4;
	int dxb =  size/4;
	int dya = int( -e*size/4 );
	int dyb = int( e*size/4 );
	int xa = x + dxa;
	int xb = x + dxb;
	int ya = y + dya;
	int yb = y + dyb;

	int psize;

	QPixmap *star;

	switch (type) {
		case 0: //star
			star = starpix->getPixmap (&color, size);

			//Only bitBlt() if we are drawing to the sky pixmap
			if ( psky.device() == sky )
				bitBlt ((QPaintDevice *) sky, x - star->width()/2, y - star->height()/2, star);
			else
				psky.drawPixmap( x - star->width()/2, y - star->height()/2, *star );
			break;
		case 1: //catalog star
			//Some NGC/IC objects are stars...changed their type to 1 (was double star)
			if (size<2) size = 2;
			psky.drawEllipse( x1, y1, size/2, size/2 );
			break;
		case 2: //Planet
			break;
		case 3: //Open cluster
			psky.setBrush( psky.pen().color() );
			psize = 2;
			if ( size > 50 )  psize *= 2;
			if ( size > 100 ) psize *= 2;
			psky.drawEllipse( xa, y1, psize, psize ); // draw circle of points
			psky.drawEllipse( xb, y1, psize, psize );
			psky.drawEllipse( xa, y2, psize, psize );
			psky.drawEllipse( xb, y2, psize, psize );
			psky.drawEllipse( x1, ya, psize, psize );
			psky.drawEllipse( x1, yb, psize, psize );
			psky.drawEllipse( x2, ya, psize, psize );
			psky.drawEllipse( x2, yb, psize, psize );
			psky.setBrush( QColor( data->options->colorScheme()->colorNamed( "SkyColor" ) ) );
			break;
		case 4: //Globular Cluster
			if (size<2) size = 2;
			psky.save();
			psky.translate( x, y );
			psky.rotate( double( pa ) );  //rotate the coordinate system
			psky.drawEllipse( dx1, dy1, size, int( e*size ) );
			psky.moveTo( 0, dy1 );
			psky.lineTo( 0, dy2 );
			psky.moveTo( dx1, 0 );
			psky.lineTo( dx2, 0 );
			psky.restore(); //reset coordinate system
			break;
		case 5: //Gaseous Nebula
			if (size <2) size = 2;
			psky.save();
			psky.translate( x, y );
			psky.rotate( double( pa ) );  //rotate the coordinate system
			psky.drawLine( dx1, dy1, dx2, dy1 );
			psky.drawLine( dx2, dy1, dx2, dy2 );
			psky.drawLine( dx2, dy2, dx1, dy2 );
			psky.drawLine( dx1, dy2, dx1, dy1 );
			psky.restore(); //reset coordinate system
			break;
		case 6: //Planetary Nebula
			if (size<2) size = 2;
			psky.save();
			psky.translate( x, y );
			psky.rotate( double( pa ) );  //rotate the coordinate system
			psky.drawEllipse( dx1, dy1, size, int( e*size ) );
			psky.moveTo( 0, dy1 );
			psky.lineTo( 0, dy1 - int( e*size/2 ) );
			psky.moveTo( 0, dy2 );
			psky.lineTo( 0, dy2 + int( e*size/2 ) );
			psky.moveTo( dx1, 0 );
			psky.lineTo( dx1 - size/2, 0 );
			psky.moveTo( dx2, 0 );
			psky.lineTo( dx2 + size/2, 0 );
			psky.restore(); //reset coordinate system
			break;
		case 7: //Supernova remnant
			if (size<2) size = 2;
			psky.save();
			psky.translate( x, y );
			psky.rotate( double( pa ) );  //rotate the coordinate system
			psky.moveTo( 0, dy1 );
			psky.lineTo( dx2, 0 );
			psky.lineTo( 0, dy2 );
			psky.lineTo( dx1, 0 );
			psky.lineTo( 0, dy1 );
			psky.restore(); //reset coordinate system
			break;
		case 8: //Galaxy
			if ( size <1 && zoomFactor() > 20*MINZOOM ) size = 3; //force ellipse above zoomFactor 20
			if ( size <1 && zoomFactor() > 5*MINZOOM ) size = 1; //force points above zoomFactor 5
			if ( size>2 ) {
				psky.save();
				psky.translate( x, y );
				psky.rotate( double( pa ) );  //rotate the coordinate system
				psky.drawEllipse( dx1, dy1, size, int( e*size ) );
				psky.restore(); //reset coordinate system

			} else if ( size>0 ) {
				psky.drawPoint( x, y );
			}
			break;
	}
}

void SkyMap::exportSkyImage( const QPaintDevice *pd ) {
	QPainter p;

	//shortcuts to inform wheter to draw different objects
	bool drawPlanets( data->options->drawPlanets );
	bool drawMW( data->options->drawMilkyWay );
	bool drawCNames( data->options->drawConstellNames );
	bool drawCLines( data->options->drawConstellLines );
	bool drawGrid( data->options->drawGrid );

	p.begin( pd );
	QPaintDeviceMetrics pdm( p.device() );

	//scale image such that it fills 90% of the x or y dimension on the paint device
	double xscale = pdm.width() / width();
	double yscale = pdm.height() / height();
	double scale = (xscale < yscale) ? xscale : yscale;

	int pdWidth = int( scale * width() );
	int pdHeight = int( scale * height() );
	int x1 = int( 0.5*(pdm.width()  - pdWidth) );
	int y1 = int( 0.5*(pdm.height()  - pdHeight) );

	p.setClipRect( QRect( x1, y1, pdWidth, pdHeight ) );
	p.setClipping( true );

	//Fil background with sky color
	p.fillRect( x1, y1, pdWidth, pdHeight, QBrush( data->options->colorScheme()->colorNamed( "SkyColor" ) ) );

	if ( x1 || y1 ) p.translate( x1, y1 );

	QFont stdFont = p.font();
	QFont smallFont = p.font();
	smallFont.setPointSize( stdFont.pointSize() - 2 );

	if ( drawMW ) drawMilkyWay( p, scale );
	if ( drawGrid ) drawCoordinateGrid( p, scale );
	if ( data->options->drawEquator ) drawEquator( p, scale );
	if ( data->options->drawEcliptic ) drawEcliptic( p, scale );
	if ( drawCLines ) drawConstellationLines( p, scale );
	if ( drawCNames ) drawConstellationNames( p, stdFont, scale );

	// stars and planets use the same font size
	if ( data->options->ZoomFactor < 10.*MINZOOM ) {
		p.setFont( smallFont );
	} else {
		p.setFont( stdFont );
	}

	drawStars( p, scale );
	drawDeepSkyObjects( p, scale );
	drawSolarSystem( p, drawPlanets, scale );
	drawAttachedLabels( p, scale );
	drawHorizon( p, stdFont, scale );

	p.end();
}

void SkyMap::setMapGeometry() {
	guidemax = int(zoomFactor()/10.0);

	isPoleVisible = false;
	if ( data->options->useAltAz ) {
		XRange = 1.2*fov()/cos( focus()->alt()->radians() );
		Ymax = fabs( focus()->alt()->Degrees() ) + fov();
	} else {
		XRange = 1.2*fov()/cos( focus()->dec()->radians() );
		Ymax = fabs( focus()->dec()->Degrees() ) + fov();
	}
	if ( Ymax >= 90. ) isPoleVisible = true;

	//at high zoom, double FOV for guide lines so they don't disappear.
	guideFOV = fov();
	guideXRange = XRange;
	if ( zoomFactor() > 10.*MINZOOM ) { guideFOV *= 2.0; guideXRange *= 2.0; }
}
