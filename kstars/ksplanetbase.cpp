/***************************************************************************
                          ksplanetbase.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
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

#include <math.h>
#include "kstars.h"
#include "ksutils.h"
#include "ksplanetbase.h"


KSPlanetBase::KSPlanetBase( KStars *ks, QString s, QString image_file )
 : SkyObject( 2, 0.0, 0.0, 0.0, s, "" ), Image(0), kstars(ks) {

	 if (! image_file.isEmpty()) {
		QFile imFile;

		if ( KSUtils::openDataFile( imFile, image_file ) ) {
			imFile.close();
			Image0.load( imFile.name() );
			Image = Image0.convertDepth( 32 );
			Image0 = Image;
		}
	}
	PositionAngle = 0.0;
	ImageAngle = 0.0;
}

void KSPlanetBase::EquatorialToEcliptic( const dms *Obliquity ) {
	findEcliptic( Obliquity, ep.longitude, ep.latitude );
}
	
void KSPlanetBase::EclipticToEquatorial( const dms *Obliquity ) {
	setFromEcliptic( Obliquity, &ep.longitude, &ep.latitude );
}

void KSPlanetBase::updateCoords( KSNumbers *num, bool includePlanets ){
	if ( includePlanets ) {
		kstars->data()->earth()->findPosition( num );
		findPosition( num, kstars->data()->earth() );
	}
}

void KSPlanetBase::rotateImage( double imAngle ) {
//Update PositionAngle and rotate Image if the new position angle (p) is
//more than 5 degrees from the stored PositionAngle.
	
//JH: 2003 Feb 28 now rotating each time, even for small changes in angle
	//if ( fabs( imAngle - ImageAngle ) > 5.0 ) {
		ImageAngle = imAngle;
		QWMatrix m;
		m.rotate( ImageAngle );
		//Image = xFormImage( m );
		Image = Image0.xForm( m );
	//}
}

/*JH: This is copied verbatim from the Qt 3.0 function QImage::xForm().  We
	copy it here for backward-compatibility with Qt/KDE 2.x (cross your fingers...)
	Changes required:
		+ Insert "Image0." in front of all QImage function calls.
		+ replaced references to dImage.data (which is private to QImage) with accessor
			functions (e.g., use dImage.numColors() instead of dImage.data.ncols)
		+ also adding qt_xForm_helper() from qpixmap.cpp (this is not a class member fcn)
		+ also adding macros used by qt_xForm_helper()
		+ changed BigEndian to QImage::BigEndian (enum value defined in qimage.h)

	!
  Returns a copy of the image that is transformed using the
  transformation matrix, \a matrix.

  The transformation \a matrix is internally adjusted to compensate
  for unwanted translation, i.e. xForm() returns the smallest image
  that contains all the transformed points of the original image.

  \sa scale() QPixmap::xForm() QPixmap::trueMatrix() QWMatrix
*/
#ifndef QT_NO_IMAGE_TRANSFORMATION
QImage KSPlanetBase::xFormImage( const QWMatrix &matrix ) const {
	// This function uses the same algorithm (and quite some code) as
	// QPixmap::xForm().

	if ( Image0.isNull() ) return Image0.copy();

	if ( Image0.depth() == 16 ) {
		// inefficient
		//JH: Image0 is guaranteed to be 32-bit image (see constructor)
		//Image0 = Image0.convertDepth(32);
		return xFormImage( matrix );
	}

	// source image data
	int ws = Image0.width();
	int hs = Image0.height();
	int sbpl = Image0.bytesPerLine();
	uchar *sptr = Image0.bits();

	// target image data
	int wd;
	int hd;

	int bpp = Image0.depth();

	// compute size of target image
	QWMatrix mat = QPixmap::trueMatrix( matrix, ws, hs );
	if ( mat.m12() == 0.0F && mat.m21() == 0.0F ) {
		if ( mat.m11() == 1.0F && mat.m22() == 1.0F ) {
			return Image0;  // identity matrix
		}
		hd = qRound( mat.m22()*hs );
		wd = qRound( mat.m11()*ws );
		hd = QABS( hd );
		wd = QABS( wd );
	} else { 				// rotation or shearing
		QPointArray a( QRect(0,0,ws,hs) );
		a = mat.map( a );
		QRect r = a.boundingRect().normalize();
		wd = r.width();
		hd = r.height();
	}

	bool invertible;
	mat = mat.invert( &invertible );		// invert matrix
	if ( hd == 0 || wd == 0 || !invertible ) {	// error, return null image
		QImage im;
		return im;
	}

	// create target image (some of the code is from QImage::copy())
	QImage dImage( wd, hd, Image0.depth(), Image0.numColors(), Image0.bitOrder() );
	memcpy( dImage.colorTable(), Image0.colorTable(), Image0.numColors()*sizeof(QRgb) );
	dImage.setAlphaBuffer( Image0.hasAlphaBuffer() );
	dImage.setDotsPerMeterX( Image0.dotsPerMeterX() );
	dImage.setDotsPerMeterY( Image0.dotsPerMeterY() );

	// initizialize the data
	switch ( bpp ) {
	case 1:
		memset( dImage.bits(), 0, dImage.numBytes() );
		break;
	case 8:
		if ( dImage.numColors() < 256 ) {
			// colors are left in the color table, so pick that one as transparent
			dImage.setNumColors( dImage.numColors()+1 );
			dImage.setColor( dImage.numColors()-1, 0x00 );
			memset( dImage.bits(), dImage.numColors()-1, dImage.numBytes() );
		} else {
			// ### What is the right thing here?
			dImage.setColor( 255, 0x00 );
			memset( dImage.bits(), 255, dImage.numBytes() );
		}
		break;
	case 16:
		memset( dImage.bits(), 0xff, dImage.numBytes() );
		break;
	case 32:
		memset( dImage.bits(), 0x00, dImage.numBytes() );
		break;
	}

	int type;
	if ( Image0.bitOrder() == QImage::BigEndian )
		type = QT_XFORM_TYPE_MSBFIRST;
	else
		type = QT_XFORM_TYPE_LSBFIRST;

	int dbpl = dImage.bytesPerLine();
	qt_xForm_helper( mat, 0, type, bpp,
			dImage.bits(),
			dbpl,
			0,
			hd, sptr, sbpl, ws, hs );
	return dImage;
}

//macros used by qt_xForm_helper()...
#undef IWX_MSB
#define IWX_MSB(b)  if ( trigx < maxws && trigy < maxhs ) {   \
				if ( *(sptr+sbpl*(trigy>>16)+(trigx>>19)) &      \
					(1 << (7-((trigx>>16)&7))) )    \
				*dptr |= b;                       \
			}                                   \
			trigx += m11;                       \
			trigy += m12;
// END OF MACRO
#undef IWX_LSB
#define IWX_LSB(b)  if ( trigx < maxws && trigy < maxhs ) {    \
				if ( *(sptr+sbpl*(trigy>>16)+(trigx>>19)) &      \
					(1 << ((trigx>>16)&7)) )        \
				*dptr |= b;                       \
			}                                   \
			trigx += m11;                       \
			trigy += m12;
// END OF MACRO
#undef IWX_PIX
#define IWX_PIX(b)  if ( trigx < maxws && trigy < maxhs ) {    \
				if ( (*(sptr+sbpl*(trigy>>16)+(trigx>>19)) &   \
					(1 << (7-((trigx>>16)&7)))) == 0 )     \
				*dptr &= ~b;                             \
			}                                          \
			trigx += m11;                              \
			trigy += m12;
// END OF MACRO

bool qt_xForm_helper( const QWMatrix &trueMat, int xoffset,
	int type, int depth,
	uchar *dptr, int dbpl, int p_inc, int dHeight,
	uchar *sptr, int sbpl, int sWidth, int sHeight
	)
{
    int m11 = (int)(trueMat.m11()*65536.0);
    int m12 = (int)(trueMat.m12()*65536.0);
    int m21 = (int)(trueMat.m21()*65536.0);
    int m22 = (int)(trueMat.m22()*65536.0);
    int dx  = (int)(trueMat.dx() *65536.0);
    int dy  = (int)(trueMat.dy() *65536.0);

    int m21ydx = dx + (xoffset<<16);
    int m22ydy = dy;
    uint trigx;
    uint trigy;
    uint maxws = sWidth<<16;
    uint maxhs = sHeight<<16;

    for ( int y=0; y<dHeight; y++ ) {		// for each target scanline
	trigx = m21ydx;
	trigy = m22ydy;
	uchar *maxp = dptr + dbpl;
	if ( depth != 1 ) {
	    switch ( depth ) {
		case 8:				// 8 bpp transform
		while ( dptr < maxp ) {
		    if ( trigx < maxws && trigy < maxhs )
			*dptr = *(sptr+sbpl*(trigy>>16)+(trigx>>16));
		    trigx += m11;
		    trigy += m12;
		    dptr++;
		}
		break;

		case 16:			// 16 bpp transform
		while ( dptr < maxp ) {
		    if ( trigx < maxws && trigy < maxhs )
			*((ushort*)dptr) = *((ushort *)(sptr+sbpl*(trigy>>16) +
						     ((trigx>>16)<<1)));
		    trigx += m11;
		    trigy += m12;
		    dptr++;
		    dptr++;
		}
		break;

		case 24: {			// 24 bpp transform
		uchar *p2;
		while ( dptr < maxp ) {
		    if ( trigx < maxws && trigy < maxhs ) {
			p2 = sptr+sbpl*(trigy>>16) + ((trigx>>16)*3);
			dptr[0] = p2[0];
			dptr[1] = p2[1];
			dptr[2] = p2[2];
		    }
		    trigx += m11;
		    trigy += m12;
		    dptr += 3;
		}
		}
		break;

		case 32:			// 32 bpp transform
		while ( dptr < maxp ) {
		    if ( trigx < maxws && trigy < maxhs )
			*((uint*)dptr) = *((uint *)(sptr+sbpl*(trigy>>16) +
						   ((trigx>>16)<<2)));
		    trigx += m11;
		    trigy += m12;
		    dptr += 4;
		}
		break;

		default: {
		return FALSE;
		}
	    }
	} else  {
	    switch ( type ) {
		case QT_XFORM_TYPE_MSBFIRST:
		    while ( dptr < maxp ) {
			IWX_MSB(1);
			IWX_MSB(2);
			IWX_MSB(4);
			IWX_MSB(8);
			IWX_MSB(16);
			IWX_MSB(32);
			IWX_MSB(64);
			IWX_MSB(128);
			dptr++;
		    }
		    break;
		case QT_XFORM_TYPE_LSBFIRST:
		    while ( dptr < maxp ) {
			IWX_LSB(1);
			IWX_LSB(2);
			IWX_LSB(4);
			IWX_LSB(8);
			IWX_LSB(16);
			IWX_LSB(32);
			IWX_LSB(64);
			IWX_LSB(128);
			dptr++;
		    }
		    break;
#  if defined(Q_WS_WIN)
		case QT_XFORM_TYPE_WINDOWSPIXMAP:
		    while ( dptr < maxp ) {
			IWX_PIX(128);
			IWX_PIX(64);
			IWX_PIX(32);
			IWX_PIX(16);
			IWX_PIX(8);
			IWX_PIX(4);
			IWX_PIX(2);
			IWX_PIX(1);
			dptr++;
		    }
		    break;
#  endif
	    }
	}
	m21ydx += m21;
	m22ydy += m22;
	dptr += p_inc;
    }
    return TRUE;
}
#undef IWX_MSB
#undef IWX_LSB
#undef IWX_PIX
#endif //QT_NO_IMAGE_TRANSFORMATION
