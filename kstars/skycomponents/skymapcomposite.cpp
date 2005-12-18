/***************************************************************************
                          skymapcomposite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "Options.h"
#include "csegment.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"

#include "equatorcomponent.h"
#include "eclipticcomponent.h"
#include "horizoncomponent.h"
#include "milkywaycomposite.h"
#include "constellationlinescomposite.h"
#include "coordinategridcomposite.h"
#include "customcatalogcomponent.h"
#include "starcomponent.h"
#include "jupitermoonscomponent.h"

SkyMapComposite::SkyMapComposite(SkyComponent *parent, KStarsData *data) : SkyComposite(parent)
{
	//Add all components
	// beware the order of adding components
	// first added component will be drawn first
	// so horizon should be one of the last components
	addComponent( new MilkyWayComposite( this, &Options::showMilkyWay ) );
	addComponent( new CoordinateGridComposite( this, &Options::showGrid ) );
	m_CBoundsComponent = new ConstellationBoundaryComponent( this, &Options::showCBounds );
	addComponent( m_CBoundsComponent );
	addComponent( new ConstellationLinesComposite( this, data ) );
	m_CNamesComponent = new ConstellationNamesComponent( this, &Options::showCNames );
	addComponent( m_CNamesComponent );
	addComponent( new EquatorComponent( this, &Options::showEquator ) );
	addComponent( new EclipticComponent( this, &Options::showEcliptic ) );

	m_DeepSkyComponent = new DeepSkyComponent( this, &Options::showDeepSky, 
		&Options::showMessier, &Options::showNGC, &Options::showIC, 
		&Options::showOther, &Options::showMessierImages );
	addComponent( m_DeepSkyComponent );
	
	//FIXME: can't use Options::showCatalog as visibility fcn, because it returns QList, not bool
	m_CustomCatalogComposite = new SkyComposite( this );
	foreach ( QString fname, Options::catalogFile() ) 
		m_CustomCatalogComposite->addComponent( new CustomCatalogComponent( this, fname, false,  &Options::showOther ) );
	
	m_StarComponent = new StarComponent( this, &Options::showStars );
	addComponent( m_StarComponent );

	m_SSComposite = new SolarSystemComposite( this, data );
	addComponent( m_SSComposite );

	addComponent( new JupiterMoonsComponent( this, &Options::showJupiter) );
	addComponent( new HorizonComponent(this, &Options::showHorizon) );

	connect( this, SIGNAL( progressText( const QString & ) ), 
					data, SIGNAL( progressText( const QString & ) ) );
}

void SkyMapComposite::init(KStarsData *data )
{
	SkyComposite::init(data);

	//Sort the list of object names
	qSort( objectNames() );
}

void SkyMapComposite::updatePlanets(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, solarSystem())
		component->updatePlanets( data, num );
}

void SkyMapComposite::updateMoons(KStarsData *data, KSNumbers *num )
{
	foreach (SkyComponent *component, solarSystem() )
		component->updateMoons( data, num );
}

bool SkyMapComposite::addTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->addTrail( o ) ) return true;
	}
	//Did not find object o
	return false;
}

bool SkyMapComposite::hasTrail( SkyObject *o, bool &found ) {
	found = false;
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->hasTrail( o, found ) ) return true;
		//It's possible we found the object, but it had no trail:
		if ( found ) return false;
	}
	//Did not find object o
	return false;
}

bool SkyMapComposite::removeTrail( SkyObject *o ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		if ( comp->removeTrail( o ) ) return true;
	}
	//Did not find object o
	return false;
}

void SkyMapComposite::clearTrailsExcept( SkyObject *exOb ) {
	foreach ( SkyComponent *comp, solarSystem() ) {
		comp->clearTrailsExcept( exOb );
	}
}

void SkyMapComposite::setFaintStarMagnitude( float newMag ) {
	m_StarComponent->setFaintMagnitude( newMag );
}

SkyObject* SkyMapComposite::findStarByGenetiveName( const QString &name ) {
	foreach( SkyObject *s, m_StarComponent->objectList() ) 
		if ( s->name2() == name ) return s;

	return 0;
}

void SkyMapComposite::addCustomCatalog( const QString &filename, bool (*visibleMethod)() ) {
	m_CustomCatalogComposite->addComponent( new CustomCatalogComponent( this, filename, false, visibleMethod ) );
}

void SkyMapComposite::reloadDeepSky( KStarsData *data ) {
	m_DeepSkyComponent->clear();
	m_DeepSkyComponent->init( data );
}

void SkyMapComposite::reloadAsteroids( KStarsData *data ) {
	m_SSComposite->reloadAsteroids( data );
}

void SkyMapComposite::reloadComets( KStarsData *data ) {
	m_SSComposite->reloadComets( data );
}

QString SkyMapComposite::constellation( SkyPoint *p ) {
	//Identify the constellation that contains point p.
	//First, find all CSegments that bracket the RA of p.
	//Then, identify the pair of these bracketing segments which bracket p in the Dec direction.
	//Each segment has two cnames, identifying the 2 constellations which the segment delineates.
	//There will be a name in common between the pair, this is the constellation that p is in.
	//
	//Corner case 1: points near the celestial poles are not bracketed by csegments.
	//Corner case 2: it is possible that *both* cnames are shared between the two segments.
	//In this case, we have to do more work to decide which is truly correct.
	
	QList<SkyPoint*> p1List, p2List;
	QStringList name1List, name2List;
	QString abbrev("");

	double pdc = p->dec()->Degrees();
	double pra(0.0); //defined in the loop, because we may modify it there

	foreach ( CSegment *seg, m_CBoundsComponent->segmentList() ) {
		SkyPoint *p1 = seg->nodes()[0];

		for ( int i=1; i< seg->nodes().size(); ++i ) {
			pra = p->ra()->Degrees();
			SkyPoint *p2 = seg->nodes()[i]; 
			double p1ra = p1->ra()->Degrees();
			double p2ra = p2->ra()->Degrees();

			if ( p1ra > 330. && p2ra < 30. ) { //wrap RA coordinate, if necessary
				p1ra -= 360.0; 
				if ( pra > 330. ) pra -= 360.;
			}
			if ( p2ra > 330. && p1ra < 30. ) { //wrap RA coordinate, if necessary
				p2ra -= 360.0; 
				if ( pra > 330. ) pra -= 360.;
			}
			
			if ( p1ra <= pra && p2ra >= pra ) { //bracketing segment?
				p1List.append( p1 );
				p2List.append( p2 );
				name1List.append( seg->name1() );
				name2List.append( seg->name2() );
				break;
			} else if ( p2ra <= pra && p1ra >= pra ) { //bracketing segment? (RA order reversed)
				p1List.append( p2 ); //make sure p1List gets the smaller bracketing RA
				p2List.append( p1 );
				name1List.append( seg->name1() );
				name2List.append( seg->name2() );
				break;
			}
			p1 = p2;
		}
	}

	//Should not happen:
	if ( p1List.count() == 0 ) {
		kdWarning() << "A: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
		return i18n("Unknown");
	}

	//Normal case: more than one segment brackets in RA.  Find segments which also bracket in Dec.
	double dupper(90.), dlower(-90.);
	int iupper(-1), ilower(-1);
	for ( int i=0; i < p1List.size(); ++i ) {
		SkyPoint p1 = *p1List[i];
		SkyPoint p2 = *p2List[i];
		
		//Find Dec value along segment at RA of p:
		double f = ( pra - p1.ra()->Degrees() ) / ( p2.ra()->Degrees() - p1.ra()->Degrees()); //fractional distance along segment
		double segDec = f*p2.dec()->Degrees() + (1.0-f)*p1.dec()->Degrees();
		if ( segDec >= pdc && segDec < dupper ) { dupper = segDec; iupper = i; }
		if ( segDec <= pdc && segDec > dlower ) { dlower = segDec; ilower = i; }
	}

	//Corner case 1: Points near one of the poles are not bracketed by segments in the Dec direction.
	//In this case, either iupper or ilower will remain at its preassigned invalid value (-1)
	//Identify the constellation by extrapolating away from the pole to the next segment.
	//This will identify which of the two names is the neighboring constellation
	//so our target constell. is the other one.
	//(note that we can't just assume Ursa Minor or Octans, because of precession: these constellations 
	//are not near the pole at all epochs
	if ( iupper == -1 ) { //point near north pole
		int ilow2(-1);
		double dlow2(-90.);
		for ( int i=0; i < p1List.size(); ++i ) {
			if ( i != ilower ) {
				SkyPoint p1 = *p1List[i];
				SkyPoint p2 = *p2List[i];
				
				//Find Dec value along segment at RA of p:
				double f = ( pra - p1.ra()->Degrees() ) / ( p2.ra()->Degrees() - p1.ra()->Degrees()); //fractional distance along segment
				double segDec = f*p2.dec()->Degrees() + (1.0-f)*p1.dec()->Degrees();
				if ( segDec > dlow2 && segDec < dlower ) { dlow2 = segDec; ilow2 = i; }
			}
		}

		if ( ilow2 == -1 ) { //whoops, what happened?
			kdWarning() << "B: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
			return i18n("Unknown");
		}

		//If name1(ilower) is the adjacent constellation, then name2(ilower) must be the answer
		if ( name1List[ ilower ] == name1List[ ilow2 ] || name1List[ ilower ] == name2List[ ilow2 ] )
			abbrev = name2List[ ilower ];

		//If name2(ilower) is the adjacent constellation, then name1(ilower) must be the answer
		else if ( name2List[ ilower ] == name1List[ ilow2 ] || name2List[ ilower ] == name2List[ ilow2 ] )
			abbrev = name1List[ ilower ];

		else { //doh!
			kdWarning() << "C: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
			return i18n("Unknown");
		}

	} else if ( ilower == -1 ) { //point near south pole
		int iup2(-1);
		double dup2(90.);
		for ( int i=0; i < p1List.size(); ++i ) {
			if ( i != iupper ) {
				SkyPoint p1 = *p1List[i];
				SkyPoint p2 = *p2List[i];
				
				//Find Dec value along segment at RA of p:
				double f = ( pra - p1.ra()->Degrees() ) / ( p2.ra()->Degrees() - p1.ra()->Degrees()); //fractional distance along segment
				double segDec = f*p2.dec()->Degrees() + (1.0-f)*p1.dec()->Degrees();
				if ( segDec < dup2 && segDec > dupper ) { dup2 = segDec; iup2 = i; }
			}
		}

		if ( iup2 == -1 ) { //whoops, what happened?
			kdWarning() << "D: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
			return i18n("Unknown");
		}

		//If name1(iupper) is the adjacent constellation, then name2(iupper) must be the answer
		if ( name1List[ iupper ] == name1List[ iup2 ] || name1List[ iupper ] == name2List[ iup2 ] )
			abbrev = name2List[ iupper ] ;

		//If name2(iupper) is the adjacent constellation, then name1(iupper) must be the answer
		else if ( name2List[ iupper ] == name1List[ iup2 ] || name2List[ iupper ] == name2List[ iup2 ] )
			abbrev = name1List[ iupper ];

		else { //doh!
			kdWarning() << "E: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
			return i18n("Unknown");
		}
	}

	//Corner case 2: Both name1 and name2 are in common in the bracketing segments
	//This can happen if a constellation is both above and below the true bracketing constellation
	//Resolve it by again extending up or down to the next segment, which will share one of the ambiguous 
	//names.  The answer is the one not shared in this next segment.
	//To ensure that there will be a next segment, go up if dec is negative, otherwise go down.
	else if ( (name1List[ iupper ] == name1List[ ilower ] || name1List[ iupper ] == name2List[ ilower ]) 
		&& (name2List[ iupper ] == name1List[ ilower ] || name2List[ iupper ] == name2List[ ilower ]) ) {
		if ( pdc < 0.0 ) { //find next segment up
			int iup2(0);
			double dup2(90.0);

			for ( int i=0; i < p1List.size(); ++i ) {
				if ( i != iupper ) {
					SkyPoint p1 = *p1List[i];
					SkyPoint p2 = *p2List[i];
					
					//Find Dec value along segment at RA of p:
					double f = ( pra - p1.ra()->Degrees() ) / ( p2.ra()->Degrees() - p1.ra()->Degrees()); //fractional distance along segment
					double segDec = f*p2.dec()->Degrees() + (1.0-f)*p1.dec()->Degrees();
					if ( segDec < dup2 && segDec > dupper ) { dup2 = segDec; iup2 = i; }
				}
			}

			//If name1(iupper) is the adjacent constellation, then name2(iupper) must be the answer
			if ( name1List[ iupper ] == name1List[ iup2 ] || name1List[ iupper ] == name2List[ iup2 ] )
				abbrev = name2List[ iupper ];
	
			//If name2(iupper) is the adjacent constellation, then name1(iupper) must be the answer
			else if ( name2List[ iupper ] == name1List[ iup2 ] || name2List[ iupper ] == name2List[ iup2 ] )
				abbrev = name1List[ iupper ];
	
			else { //doh!
				kdWarning() << "F: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
				return i18n("Unknown");
			}
		} else { //pdc > 0.0, so search down
			int ilow2(-1);
			double dlow2(-90.);
			for ( int i=0; i < p1List.size(); ++i ) {
				if ( i != ilower ) {
					SkyPoint p1 = *p1List[i];
					SkyPoint p2 = *p2List[i];
					
					//Find Dec value along segment at RA of p:
					double f = ( pra - p1.ra()->Degrees() ) / ( p2.ra()->Degrees() - p1.ra()->Degrees()); //fractional distance along segment
					double segDec = f*p2.dec()->Degrees() + (1.0-f)*p1.dec()->Degrees();
					if ( segDec > dlow2 && segDec < dlower ) { dlow2 = segDec; ilow2 = i; }
				}
			}
	
			if ( ilow2 == -1 ) { //whoops, what happened?
				kdWarning() << "G: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
				return i18n("Unknown");
			}
	
			//If name1(ilower) is the adjacent constellation, then name2(ilower) must be the answer
			if ( name1List[ ilower ] == name1List[ ilow2 ] || name1List[ ilower ] == name2List[ ilow2 ] )
				abbrev = name2List[ ilower ];
	
			//If name2(ilower) is the adjacent constellation, then name1(ilower) must be the answer
			else if ( name2List[ ilower ] == name1List[ ilow2 ] || name2List[ ilower ] == name2List[ ilow2 ] )
				abbrev = name1List[ ilower ];
	
			else { //doh!
				kdWarning() << "H: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
				return i18n("Unknown");
			}
		}
	}

	//Normal case: one of the pair of names (name1/name2) should be shared between 
	//the two bracketing segments
	else if ( name1List[ iupper ] == name1List[ ilower ] || name1List[ iupper ] == name2List[ ilower ] ) 
		abbrev = name1List[ iupper ];

	else if ( name2List[ iupper ] == name1List[ ilower ] || name2List[ iupper ] == name2List[ ilower ] ) 
		abbrev = name2List[ iupper ];

	//If we reach here, then neither name1 nor name2 were shared between the bracketing segments!
	else {
		kdWarning() << "I: " << i18n("No constellation found for point: (%1, %2)").arg( p->ra()->toHMSString() ).arg( p->dec()->toDMSString() ) << endl;
		return i18n("Unknown");
	}

	//Finally, match the abbreviated name to the full constellation name, and return that name
	foreach ( SkyObject *o, m_CNamesComponent->objectList() ) {
		if ( abbrev.lower() == o->name2().lower() ) {
			QString r = i18n( "Constellation name (optional)", o->name().local8Bit().data() );
			r = r.left(1) + r.mid(1).lower(); //lowercase letters (except first letter)
			int i = r.find(" ");
			i++;
			if ( i>0 ) r = r.left(i) + r.mid(i,1).upper() + r.mid(i+1); //capitalize 2nd word
			return r;
		}
	}

	return i18n("Unknown");
}

void SkyMapComposite::emitProgressText( const QString &message ) {
	emit progressText( message );
}

#include "skymapcomposite.moc"
