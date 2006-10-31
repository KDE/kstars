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
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"

#include "skymapcomposite.h"

#include "constellationboundarycomponent.h"
#include "constellationlinescomposite.h"
//#include "constellationnamescomponent.h"
#include "coordinategridcomposite.h"
#include "customcatalogcomponent.h"
//#include "deepskycomponent.h"
#include "equatorcomponent.h"
#include "eclipticcomponent.h"
#include "horizoncomponent.h"
#include "jupitermoonscomponent.h"
#include "milkywaycomposite.h"
//#include "solarsystemcomposite.h"
//#include "starcomponent.h"
//#include "telescopecomponent.h"

SkyMapComposite::SkyMapComposite(SkyComponent *parent, KStarsData *data) : SkyComposite(parent)
{
	//Add all components
	// beware the order of adding components
	// first added component will be drawn first
	// so horizon should be one of the last components
	m_MilkyWay = new MilkyWayComposite( this, &Options::showMilkyWay );
	addComponent( m_MilkyWay );

	m_Stars = new StarComponent( this, &Options::showStars );
	addComponent( m_Stars );

	m_CoordinateGrid = new CoordinateGridComposite( this, &Options::showGrid );
	addComponent( m_CoordinateGrid );
	m_CBounds = new ConstellationBoundaryComponent( this, &Options::showCBounds );
	addComponent( m_CBounds );
	m_CLines = new ConstellationLinesComposite( this, data );
	addComponent( m_CLines );
	m_CNames = new ConstellationNamesComponent( this, &Options::showCNames );
	addComponent( m_CNames );
	m_Equator = new EquatorComponent( this, &Options::showEquator );
	addComponent( m_Equator );
	m_Ecliptic = new EclipticComponent( this, &Options::showEcliptic );
	addComponent( m_Ecliptic );
	m_Horizon = new HorizonComponent(this, &Options::showHorizon);
	addComponent( m_Horizon );

	m_DeepSky = new DeepSkyComponent( this, &Options::showDeepSky, 
			&Options::showMessier, &Options::showNGC, &Options::showIC, 
			&Options::showOther, &Options::showMessierImages );
	addComponent( m_DeepSky );
	//FIXME: can't use Options::showCatalog as visibility fcn, 
	//because it returns QList, not bool
	m_CustomCatalogs = new SkyComposite( this );
	foreach ( QString fname, Options::catalogFile() ) 
		m_CustomCatalogs->addComponent( new CustomCatalogComponent( this, fname, false,  &Options::showOther ) );
	
	m_SolarSystem = new SolarSystemComposite( this, data );
	addComponent( m_SolarSystem );

	connect( this, SIGNAL( progressText( const QString & ) ), 
					data, SIGNAL( progressText( const QString & ) ) );
}

void SkyMapComposite::update(KStarsData *data, KSNumbers *num )
{
	//1. Milky Way
	m_MilkyWay->update( data, num );
	//2. Coordinate grid
	m_CoordinateGrid->update( data, num );
	//3. Constellation boundaries
	m_CBounds->update( data, num );
	//4. Constellation lines
	m_CLines->update( data, num );
	//5. Constellation names
	m_CNames->update( data, num );
	//6. Equator
	m_Equator->update( data, num );
	//7. Ecliptic
	m_Ecliptic->update( data, num );
	//8. Deep sky
	m_DeepSky->update( data, num );
	//9. Custom catalogs
	m_CustomCatalogs->update( data, num );
	//10. Stars
	m_Stars->update( data, num );
	//11. Horizn
	m_Horizon->update( data, num );
}

void SkyMapComposite::updatePlanets(KStarsData *data, KSNumbers *num )
{
	m_SolarSystem->updatePlanets( data, num );
}

void SkyMapComposite::updateMoons(KStarsData *data, KSNumbers *num )
{
	m_SolarSystem->updateMoons( data, num );
}

//Reimplement draw function so that we have control over the order of 
//elements, and we can add object labels
//
//The order in which components are drawn naturally determines the 
//z-ordering (the layering) of the components.  Objects which 
//should appear "behind" others should be drawn first.
void SkyMapComposite::draw(KStars *ks, QPainter& psky, double scale)
{
	//TIMING
//	QTime t;
	//1. Milky Way
//	t.start();
	m_MilkyWay->draw( ks, psky, scale );
//	kDebug() << QString("Milky Way  : %1 ms").arg( t.elapsed() ) << endl;

	//2. Coordinate grid
//	t.start();
	m_CoordinateGrid->draw( ks, psky, scale );
//	kDebug() << QString("Coord grid : %1 ms").arg( t.elapsed() ) << endl;

	//3. Constellation boundaries
//	t.start();
	m_CBounds->draw( ks, psky, scale );
//	kDebug() << QString("Cons Bound : %1 ms").arg( t.elapsed() ) << endl;

	//4. Constellation lines
//	t.start();
	m_CLines->draw( ks, psky, scale );
//	kDebug() << QString("Cons Lines : %1 ms").arg( t.elapsed() ) << endl;

	//5. Constellation names
//	t.start();
	m_CNames->draw( ks, psky, scale );
//	kDebug() << QString("Cons Names : %1 ms").arg( t.elapsed() ) << endl;

	//6. Equator
//	t.start();
	m_Equator->draw( ks, psky, scale );
//	kDebug() << QString("Equator     : %1 ms").arg( t.elapsed() ) << endl;

	//7. Ecliptic
//	t.start();
	m_Ecliptic->draw( ks, psky, scale );
//	kDebug() << QString("Ecliptic    : %1 ms").arg( t.elapsed() ) << endl;

	//8. Deep sky
//	t.start();
	m_DeepSky->draw( ks, psky, scale );
//	kDebug() << QString("Deep sky    : %1 ms").arg( t.elapsed() ) << endl;

	//9. Custom catalogs
//	t.start();
	m_CustomCatalogs->draw( ks, psky, scale );
//	kDebug() << QString("Custom cat  : %1 ms").arg( t.elapsed() ) << endl;

	//10. Stars
//	t.start();
	m_Stars->draw( ks, psky, scale );
//	kDebug() << QString("Stars       : %1 ms").arg( t.elapsed() ) << endl;

	//11. Solar system
//	t.start();
	m_SolarSystem->draw( ks, psky, scale );
//	kDebug() << QString("Solar sys   : %1 ms").arg( t.elapsed() ) << endl;

	//Draw object name labels
//	t.start();
	ks->map()->drawObjectLabels( labelObjects(), psky, scale );
//	kDebug() << QString("Name labels : %1 ms").arg( t.elapsed() ) << endl;

	//13. Horizon (and ground)
//	t.start();
	m_Horizon->draw( ks, psky, scale );
//	kDebug() << QString("Horizon     : %1 ms").arg( t.elapsed() ) << endl;
}

//Select nearest object to the given skypoint, but give preference 
//to certain object types.
//we multiply each object type's smallest angular distance by the 
//following factors before selecting the final nearest object:
// faint stars = 1.0 (not weighted)
// stars brighter than 4th mag = 0.75
// IC catalog = 0.8
// NGC catalog = 0.6
// "other" catalog = 0.6
// Messier object = 0.5
// custom object = 0.5
// Solar system = 0.25
SkyObject* SkyMapComposite::objectNearest( SkyPoint *p, double &maxrad ) {
	double rTry = maxrad;
	double rBest = maxrad;
	SkyObject *oTry = 0;
	SkyObject *oBest = 0;

	oBest = m_Stars->objectNearest( p, rBest );
	//reduce rBest by 0.75 for stars brighter than 4th mag
	if ( oBest && oBest->mag() < 4.0 ) rBest *= 0.75;

	//m_DeepSky internally discriminates among deepsky catalogs
	//and renormalizes rTry
	oTry = m_DeepSky->objectNearest( p, rTry ); 
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;
	oTry = m_CustomCatalogs->objectNearest( p, rTry );
	rTry *= 0.5;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;
	oTry = m_SolarSystem->objectNearest( p, rTry );
	rTry *= 0.25;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	maxrad = rBest;
	return oBest; //will be 0 if no object nearer than maxrad was found

}

bool SkyMapComposite::addNameLabel( SkyObject *o ) {
	if ( !o ) return false;
	labelObjects().append( o );
	return true;
}

bool SkyMapComposite::removeNameLabel( SkyObject *o ) {
	if ( !o ) return false;
	int index = labelObjects().indexOf( o );
	if ( index < 0 ) return false;
	labelObjects().removeAt( index );
	return true;
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
	m_Stars->setFaintMagnitude( newMag );
}

void SkyMapComposite::setStarColorMode( int newMode ) {
	m_Stars->setStarColorMode( newMode );
}

void SkyMapComposite::setStarColorIntensity( int newIntensity ) {
	m_Stars->setStarColorIntensity( newIntensity );
}

SkyObject* SkyMapComposite::findByName( const QString &name ) {
	//We search the children in an "intelligent" order (most-used 
	//object types first), in order to avoid wasting too much time 
	//looking for a match.  The most important part of this ordering 
	//is that stars should be last (because the stars list is so long)
	SkyObject *o = 0;
	o = m_SolarSystem->findByName( name );
	if ( o ) return o;
	o = m_DeepSky->findByName( name );
	if ( o ) return o;
	o = m_CustomCatalogs->findByName( name );
	if ( o ) return o;
	o = m_Stars->findByName( name );
	if ( o ) return o;

	return 0;
}

SkyObject* SkyMapComposite::findStarByGenetiveName( const QString &name ) {
	foreach( SkyObject *s, m_Stars->objectList() ) 
		if ( s->name2() == name ) return s;

	return 0;
}

void SkyMapComposite::addCustomCatalog( const QString &filename, bool (*visibleMethod)() ) {
	m_CustomCatalogs->addComponent( new CustomCatalogComponent( this, filename, false, visibleMethod ) );
}

void SkyMapComposite::removeCustomCatalog( const QString &name ) {
  foreach( SkyComponent *sc, m_CustomCatalogs->components() ) {
    CustomCatalogComponent *ccc = (CustomCatalogComponent*)sc;

    if ( ccc->name() == name ) {
      m_CustomCatalogs->removeComponent( ccc );
      return;
    }
  }
  
  kWarning() << i18n( "Could not find custom catalog component named %1." , name) << endl;
}

void SkyMapComposite::reloadDeepSky( KStarsData *data ) {
	m_DeepSky->clear();
	m_DeepSky->init( data );
}

void SkyMapComposite::reloadAsteroids( KStarsData *data ) {
	m_SolarSystem->reloadAsteroids( data );
}

void SkyMapComposite::reloadComets( KStarsData *data ) {
	m_SolarSystem->reloadComets( data );
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
	QString abbrev;

	double pdc = p->dec()->Degrees();
	double pra(0.0); //defined in the loop, because we may modify it there

	foreach ( CSegment *seg, m_CBounds->segmentList() ) {
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
		kWarning() << "A: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
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
			kWarning() << "B: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
			return i18n("Unknown");
		}

		//If name1(ilower) is the adjacent constellation, then name2(ilower) must be the answer
		if ( name1List[ ilower ] == name1List[ ilow2 ] || name1List[ ilower ] == name2List[ ilow2 ] )
			abbrev = name2List[ ilower ];

		//If name2(ilower) is the adjacent constellation, then name1(ilower) must be the answer
		else if ( name2List[ ilower ] == name1List[ ilow2 ] || name2List[ ilower ] == name2List[ ilow2 ] )
			abbrev = name1List[ ilower ];

		else { //doh!
			kWarning() << "C: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
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
			kWarning() << "D: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
			return i18n("Unknown");
		}

		//If name1(iupper) is the adjacent constellation, then name2(iupper) must be the answer
		if ( name1List[ iupper ] == name1List[ iup2 ] || name1List[ iupper ] == name2List[ iup2 ] )
			abbrev = name2List[ iupper ] ;

		//If name2(iupper) is the adjacent constellation, then name1(iupper) must be the answer
		else if ( name2List[ iupper ] == name1List[ iup2 ] || name2List[ iupper ] == name2List[ iup2 ] )
			abbrev = name1List[ iupper ];

		else { //doh!
			kWarning() << "E: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
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
				kWarning() << "F: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
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
				kWarning() << "G: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
				return i18n("Unknown");
			}
	
			//If name1(ilower) is the adjacent constellation, then name2(ilower) must be the answer
			if ( name1List[ ilower ] == name1List[ ilow2 ] || name1List[ ilower ] == name2List[ ilow2 ] )
				abbrev = name2List[ ilower ];
	
			//If name2(ilower) is the adjacent constellation, then name1(ilower) must be the answer
			else if ( name2List[ ilower ] == name1List[ ilow2 ] || name2List[ ilower ] == name2List[ ilow2 ] )
				abbrev = name1List[ ilower ];
	
			else { //doh!
				kWarning() << "H: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
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
		kWarning() << "I: " << i18n("No constellation found for point: (%1, %2)", p->ra()->toHMSString(), p->dec()->toDMSString() ) << endl;
		return i18n("Unknown");
	}

	//Finally, match the abbreviated name to the full constellation name, and return that name
	foreach ( SkyObject *o, m_CNames->objectList() ) {
		if ( abbrev.toLower() == o->name2().toLower() ) {
			QString r = i18nc( "Constellation name (optional)", o->name().toLocal8Bit().data() );
			r = r.left(1) + r.mid(1).toLower(); //lowercase letters (except first letter)
			int i = r.indexOf(" ");
			i++;
			if ( i>0 ) r = r.left(i) + r.mid(i,1).toUpper() + r.mid(i+1); //capitalize 2nd word
			return r;
		}
	}

	return i18n("Unknown");
}

void SkyMapComposite::emitProgressText( const QString &message ) {
	emit progressText( message );
}

#include "skymapcomposite.moc"
