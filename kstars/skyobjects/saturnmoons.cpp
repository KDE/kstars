/***************************************************************************
                          saturnmoons.cpp  -  description
                             -------------------
    begin                : Sat Mar 13 2009
                             : by Vipul Kumar Singh
    email                : vipulkrsingh@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "saturnmoons.h"

#include <kdebug.h>

#include "ksnumbers.h"
#include "ksplanet.h"
#include "kssun.h"
#include "trailobject.h"

SaturnMoons::SaturnMoons(){
    //Initialize the Moon objects.  The magnitude data are from the 
    //wikipedia articles for each moon, as of Mar 2009.
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 12.9, i18nc( "Saturn's moon Mimas", "Mimas" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 11.7, i18nc( "Saturn's moon Enceladus", "Enceladus" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 10.2, i18nc( "Saturn's moon Tethys", "Tethys" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 10.4, i18nc( "Saturn's moon Dione", "Dione" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 10, i18nc( "Saturn's moon Rheas", "Rhea" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 7.9, i18nc( "Saturn's moon Titan", "Titan" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 14.9, i18nc( "Saturn's moon Hyperion", "Hyperion" ) ) );
    Moon.append( new TrailObject( SkyObject::MOON, 0.0, 0.0, 11, i18nc( "Saturn's moon Iapetus", "Iapetus" ) ) );

    for ( uint i=0; i<8; ++i ) {
        XS[i] = 0.0;
        YS[i] = 0.0;
        ZS[i] = 0.0;
    }
}

SaturnMoons::~SaturnMoons(){
    qDeleteAll( Moon );
}

QString SaturnMoons::name( int id ) const { 
    return Moon[id]->translatedName(); 
}

TrailObject* SaturnMoons::moonNamed( const QString &name ) const {
    for ( uint i=0; i<8; ++i ) {
        if ( Moon[i]->name() == name ) {
            return Moon[i];
            break;
        }
    }
    return 0;
}

void SaturnMoons::EquatorialToHorizontal( const dms *LST, const dms *lat ) {
    for ( uint i=0; i<8; ++i )
        moon(i)->EquatorialToHorizontal( LST, lat );
}
void SaturnMoons::HelperSubroutine(double e, double lambdadash, double p, double a, double omega, double i, double c1, double s1, double& r, double& lambda, double& gamma, double& w)
{
  double e2 = e*e;
  double e3 = e2*e;
  double e4 = e3*e;
  double e5 = e4*e;
  double M = (lambdadash - p)*dms::DegToRad;
 
  double Crad = (2*e - 0.25*e3 + 0.0520833333*e5)*sin(M) +
             (1.25*e2 - 0.458333333*e4)*sin(2*M) +
             (1.083333333*e3 - 0.671875*e5)*sin(3*M) +
             1.072917*e4*sin(4*M) + 1.142708*e5*sin(5*M);
  double C = (Crad)/(dms::DegToRad);
  r = a*(1 - e2)/(1 + e*cos(M + Crad));
  double g = omega - 168.8112;
  double grad = (g)*dms::DegToRad;
  double irad = (i)*dms::DegToRad;
  double a1 = sin(irad)*sin(grad);
  double a2 = c1*sin(irad)*cos(grad) - s1*cos(irad);
  gamma = (asin(sqrt(a1*a1 + a2*a2)))/(dms::DegToRad);
  double urad = atan2(a1, a2);
  double u = (urad)/(dms::DegToRad);
  w = MapTo0To360Range(168.8112 + u);
  double h = c1*sin(irad) - s1*cos(irad)*cos(grad);
  double psirad = atan2(s1*sin(grad), h);
  double psi = (psirad)/(dms::DegToRad);
  lambda = lambdadash + C + u - g - psi;
}

void SaturnMoons::findPosition( const KSNumbers *num, const KSPlanet *Saturn, const KSSun *Sun ) {
    double Xs, Ys, Zs, Rs;
    double sinJB, cosJB, sinJL, cosJL;
    double sinSB, cosSB, sinSL, cosSL;
    double D, t, tdelay, LAMBDA, ALPHA;
    double l=0;
    //double T, oj, fj, ij, pa, tb, I, P;

    //double l1, l2, l3, l4, p1, p2, p3, p4, G, fl, z, Gj, Gs, Pj;

    //L/B = true longitude/latitude of satellites
    //double S1, S2, S3, S4, L1, L2, L3, L4, b1, b2, b3, b4, R1, R2, R3, R4;
    double X[9], Y[9], Z[9];
    double A1[9], B1[9], C1[9];
    double A2[9], B2[9], C2[9];
    double A3[9], B3[9], C3[9];
    double A4[9], B4[9], C4[9];

    Saturn->ecLong()->SinCos( sinJL, cosJL );
    Saturn->ecLat()->SinCos( sinJB, cosJB );

    Sun->ecLong()->SinCos( sinSL, cosSL );
    Sun->ecLat()->SinCos( sinSB, cosSB );

    //Geocentric Rectangular coordinates of Saturn:
    Xs = Saturn->rsun() * cosJB *cosJL + Sun->rsun() * cosSL;
    Ys = Saturn->rsun() * cosJB *sinJL + Sun->rsun() * sinSL;
    Zs = Saturn->rsun() * sinJB;

    //Distance and light-travel delay time:
    //0.0057755183 is the inverse of the speed of light, in days/AU
    Rs = sqrt(Xs*Xs +Ys*Ys + Zs*Zs );
    tdelay = 0.0057755183*Rs;  //light travel delay, in days

    LAMBDA = atan2(Ys, Xs);
    
    ALPHA = atan2( Zs, sqrt( Xs*Xs + Ys*Ys ) );

    //days since 10 Aug 1976 0h (minus light-travel delay)
    t = num->julianDay() - tdelay;

	double t1=t-2411093.0;
	double t2=t1/365.25;
	double t3=((t-2433282.423)/365.25)+1950.0;
	double t4=t-2411368.0;
	double t5=t4/365.25;
	double t6=t-2415020.0;
	double t7=t6/36525.0;
	double t8=t6/365.25;
	double t9=(t-2442000.5)/365.25;
	double t10=t-2409786.0;
	double t11=t10/36525.0;
	double t112=t11*t11;
	double t113=t112*t11;
	
    //Longitudes of the satellites' nodes on the equatorial plane of Jupiter

	double W0rad = MapTo0To360Range(5.095*(t3 - 1866.39))*dms::DegToRad;
	double W1rad = MapTo0To360Range(74.4- 32.39*t2)*dms::DegToRad;
	double W2rad = MapTo0To360Range(134.3 - 92.62*t2)*dms::DegToRad;
	double W3rad = MapTo0To360Range(42.0 - 0.5118*t5)*dms::DegToRad;
	double W4    = MapTo0To360Range(276.59 - 0.5118*t5);
	double W4rad = W4 * dms::DegToRad;
	double W5    = MapTo0To360Range(267.2635- 1222.1136*t7);
	double W5rad = W5 * dms::DegToRad;
	double W6rad = MapTo0To360Range(175.4762 - 1221.5515*t7)*dms::DegToRad;
	double W7rad = MapTo0To360Range(2.4891 - 0.002435*t7)*dms::DegToRad;
	double W8rad = MapTo0To360Range(113.35 - 0.2597*t7)*dms::DegToRad;
	
	
	double s1=sin((28.0817)*dms::DegToRad);
	double s2=sin((168.8112)*dms::DegToRad);
	double c1=cos((28.0817)*dms::DegToRad); 
	double c2=cos((168.8112)*dms::DegToRad);
	double e1= 0.05589 - 0.000346*t7;
	
  //Satellite 1
  double L = MapTo0To360Range(127.64 + 381.994497*t1 - 43.57*sin(W0rad) - 0.720*sin(3*W0rad) - 0.02144*sin(5*W0rad));
  double p = 106.1 + 365.549*t2;
  double M = L - p;
  double Mrad = (M)*dms::DegToRad;
  double C = (2.18287*sin(Mrad) + 0.025988*sin(2*Mrad) + 0.00043*sin(3*Mrad));
  double Crad = (C)*dms::DegToRad;
  double lambda1 = MapTo0To360Range(L + C);
  double r1 = 3.06879/(1 + 0.01905*cos(Mrad + Crad));
  double gamma1 = 1.563;
  double omega1 = MapTo0To360Range(54.5 - 365.072*t2);
	
    //Satellite 2
  L = MapTo0To360Range(200.317 + 262.7319002*t1 + 0.25667*sin(W1rad) + 0.20883*sin(W2rad));
  p = 309.107 + 123.44121*t2;
  M = (L - p);
  Mrad = (M)*dms::DegToRad;
  C = 0.55577*sin(Mrad) + 0.00168*sin(2*Mrad);
  Crad = (C)*dms::DegToRad;
  double lambda2 = MapTo0To360Range(L + C);
  double r2 = 3.94118/(1 + 0.00485*cos(Mrad + Crad));
  double gamma2 = 0.0262;
  double omega2 = MapTo0To360Range(348 - 151.95*t2);
  
    //Satellite 3
  double lambda3 = MapTo0To360Range(285.306 + 190.69791226*t1 + 2.063*sin(W0rad) + 0.03409*sin(3*W0rad) + 0.001015*sin(5*W0rad));
  double r3 = 4.880998;
  double gamma3 = 1.0976;
  double omega3 = MapTo0To360Range(111.33 - 72.2441*t2);

    //Satellite 4
  L = MapTo0To360Range(254.712 + 131.53493193*t1 - 0.0215*sin(W1rad) - 0.01733*sin(W2rad));
  p = 174.8 + 30.820*t2;
  M = L - p;
  Mrad = (M)*dms::DegToRad;
  C = 0.24717*sin(Mrad) + 0.00033*sin(2*Mrad);
  Crad = (C)*dms::DegToRad;
  double lambda4 = MapTo0To360Range(L + C);
  double r4 = 6.24871/(1 + 0.002157*cos(Mrad + Crad));
  double gamma4 = 0.0139;
  double omega4 = MapTo0To360Range(232 - 30.27*t2);
  
   //Satellite 5
  double pdash = 342.7 + 10.057*t2;
  double pdashrad = (pdash)*dms::DegToRad;
  double a1 = 0.000265*sin(pdashrad) + 0.001*sin(W4rad); //Note the book uses the incorrect constant 0.01*sin(W4rad);
  double a2 = 0.000265*cos(pdashrad) + 0.001*cos(W4rad); //Note the book uses the incorrect constant 0.01*cos(W4rad);
  double e = sqrt(a1*a1 + a2*a2);
  p =(atan2(a1, a2))/(dms::DegToRad);
  double N = 345 - 10.057*t2;
  double Nrad = (N)*dms::DegToRad;
  double lambdadash = MapTo0To360Range(359.244 + 79.69004720*t1 + 0.086754*sin(Nrad));
  double i = 28.0362 + 0.346898*cos(Nrad) + 0.01930*cos(W3rad);
  double omega = 168.8034 + 0.736936*sin(Nrad) + 0.041*sin(W3rad);
  double a = 8.725924;
  double lambda5 = 0;
  double gamma5 = 0;
  double omega5 = 0;
  double r5 = 0;
  HelperSubroutine(e, lambdadash, p, a, omega, i, c1, s1, r5, lambda5, gamma5, omega5);
 
//Satellite 6
  L = 261.1582 + 22.57697855*t4 + 0.074025*sin(W3rad);
  double idash = 27.45141 + 0.295999*cos(W3rad);
  double idashrad = (idash)*dms::DegToRad;
  double omegadash = 168.66925 + 0.628808*sin(W3rad);
  double omegadashrad = (omegadash)*dms::DegToRad;
  a1 = sin(W7rad)*sin(omegadashrad - W8rad);
  a2 = cos(W7rad)*sin(idashrad) - sin(W7rad)*cos(idashrad)*cos(omegadashrad - W8rad);
  double g0 = (102.8623)*dms::DegToRad;
  double psi = atan2(a1, a2);
  if (a2 < 0)
    psi += dms::PI;
  double psideg = (psi)/(dms::DegToRad);
  double s = sqrt(a1*a1 + a2*a2);
  double g = W4 - omegadash - psideg;
  double w_ = 0;
  for (int j=0; j<3; j++)
  {
    w_ = W4 + 0.37515*(sin(2*((g)*dms::DegToRad)) - sin(2*g0));
    g = w_ - omegadash - psideg;
  }
  double grad = (g)*dms::DegToRad;
  double edash = 0.029092 + 0.00019048*(cos(2*grad) - cos(2*g0));
  double q = (2*(W5 - w_))*dms::DegToRad;
  double b1 = sin(idashrad)*sin(omegadashrad - W8rad);
  double b2 = cos(W7rad)*sin(idashrad)*cos(omegadashrad - W8rad) - sin(W7rad)*cos(idashrad);
  double atanb1b2 = atan2(b1, b2);
  double theta = atanb1b2 + W8rad;
  e = edash + 0.002778797*edash*cos(q);
  p = w_ + 0.159215*sin(q);
  double u = 2*W5 - 2*theta + psi;
  double h = 0.9375*edash*edash*sin(q) + 0.1875*s*s*sin(2*(W5rad - theta));
  lambdadash = MapTo0To360Range(L - 0.254744*(e1*sin(W6rad) + 0.75*e1*e1*sin(2*W6rad) + h));
  i = idash + 0.031843*s*cos(u);
  omega = omegadash + (0.031843*s*sin(u))/sin(idashrad);
  a = 20.216193;
  double lambda6 = 0;
  double gamma6 = 0;
  double omega6 = 0;
  double r6 = 0;
  HelperSubroutine(e, lambdadash, p, a, omega, i, c1, s1, r6, lambda6, gamma6, omega6); 

 //Satellite 7
  double eta = 92.39 + 0.5621071*t6;
  double etarad = (eta)*dms::DegToRad;
  double zeta = 148.19 - 19.18*t8;
  double zetarad = (zeta)*dms::DegToRad;
  theta = (184.8 - 35.41*t9)*dms::DegToRad;
  double thetadash = theta - (7.5)*dms::DegToRad;
  double as = (176 + 12.22*t8)*dms::DegToRad;
  double bs = (8 + 24.44*t8)*dms::DegToRad;
  double cs = bs + (5)*dms::DegToRad;
  w_ = 69.898 - 18.67088*t8;
  double phi = 2*(w_ - W5);
  double phirad = (phi)*dms::DegToRad;
  double chi = 94.9 - 2.292*t8;
  double chirad = (chi)*dms::DegToRad;
  a = 24.50601 - 0.08686*cos(etarad) - 0.00166*cos(zetarad + etarad) + 0.00175*cos(zetarad - etarad);
  e = 0.103458 - 0.004099*cos(etarad) - 0.000167*cos(zetarad + etarad) + 0.000235*cos(zetarad - etarad) +
      0.02303*cos(zetarad) - 0.00212*cos(2*zetarad) + 0.000151*cos(3*zetarad) + 0.00013*cos(phirad);
  p = w_ + 0.15648*sin(chirad) - 0.4457*sin(etarad) - 0.2657*sin(zetarad + etarad) +
      -0.3573*sin(zetarad - etarad) - 12.872*sin(zetarad) + 1.668*sin(2*zetarad) +
      -0.2419*sin(3*zetarad) - 0.07*sin(phirad);
  lambdadash = MapTo0To360Range(177.047 + 16.91993829*t6 + 0.15648*sin(chirad) + 9.142*sin(etarad) +
               0.007*sin(2*etarad) - 0.014*sin(3*etarad) + 0.2275*sin(zetarad + etarad) +
               0.2112*sin(zetarad - etarad) - 0.26*sin(zetarad) - 0.0098*sin(2*zetarad) +
               -0.013*sin(as) + 0.017*sin(bs) - 0.0303*sin(phirad));
  i = 27.3347 + 0.643486*cos(chirad) + 0.315*cos(W3rad) + 0.018*cos(theta) - 0.018*cos(cs);
  omega = 168.6812 + 1.40136*cos(chirad) + 0.68599*sin(W3rad) - 0.0392*sin(cs) + 0.0366*sin(thetadash);
  double lambda7 = 0;
  double gamma7 = 0;
  double omega7 = 0;
  double r7 = 0;
  HelperSubroutine(e, lambdadash, p, a, omega, i, c1, s1, r7, lambda7, gamma7, omega7); 
 
 //Satellite 8
  L = MapTo0To360Range(261.1582 + 22.57697855*t4);
  double w_dash = 91.796 + 0.562*t7;
  psi = 4.367 - 0.195*t7;
  double psirad = (psi)*dms::DegToRad;
  theta = 146.819 - 3.198*t7;
  phi = 60.470 + 1.521*t7;
  phirad = (phi)*dms::DegToRad;
  double PHI = 205.055 - 2.091*t7;
  edash = 0.028298 + 0.001156*t11;
  double w_0 = 352.91 + 11.71*t11;
  double mu = MapTo0To360Range(76.3852 + 4.53795125*t10);
  mu = MapTo0To360Range(189097.71668440815);
  idash = 18.4602 - 0.9518*t11 - 0.072*t112 + 0.0054*t113;
  idashrad = (idash)*dms::DegToRad;
  omegadash = 143.198 - 3.919*t11 + 0.116*t112 + 0.008*t113;
  l = (mu - w_0)*dms::DegToRad;
  g = (w_0 - omegadash - psi)*dms::DegToRad;
  double g1 = (w_0 - omegadash - phi)*dms::DegToRad;
  double ls = (W5 - w_dash)*dms::DegToRad;
  double gs = (w_dash - theta)*dms::DegToRad;
  double lt = (L - W4)*dms::DegToRad;
  double gt = (W4 - PHI)*dms::DegToRad;
  double u1 = 2*(l + g - ls - gs);
  double u2 = l + g1 - lt - gt;
  double u3 = l + 2*(g - ls - gs);
  double u4 = lt + gt - g1;
  double u5 = 2*(ls + gs);
  a = 58.935028 + 0.004638*cos(u1) + 0.058222*cos(u2);
  e = edash - 0.0014097*cos(g1 - gt) + 0.0003733*cos(u5 - 2*g) +
      0.0001180*cos(u3) + 0.0002408*cos(l) + 
      0.0002849*cos(l + u2) + 0.0006190*cos(u4);
  double w = 0.08077*sin(g1 - gt) + 0.02139*sin(u5 - 2*g) - 0.00676*sin(u3) +
      0.01380*sin(l) + 0.01632*sin(l + u2) + 0.03547*sin(u4);
  p = w_0 + w/edash;
  lambdadash = mu - 0.04299*sin(u2) - 0.00789*sin(u1) - 0.06312*sin(ls) +
               -0.00295*sin(2*ls) - 0.02231*sin(u5) + 0.00650*sin(u5 + psirad);
  i = idash + 0.04204*cos(u5 + psirad) + 0.00235*cos(l + g1 + lt + gt + phirad) +
      0.00360*cos(u2 + phirad);
  double wdash = 0.04204*sin(u5 + psirad) + 0.00235*sin(l + g1 + lt + gt + phirad) +
       0.00358*sin(u2 + phirad);
  omega = omegadash + wdash/sin(idashrad);
  double lambda8 = 0;
  double gamma8 = 0;
  double omega8 = 0;
  double r8 = 0;
  HelperSubroutine(e, lambdadash, p, a, omega, i, c1, s1, r8, lambda8, gamma8, omega8);
 
  u = (lambda1 - omega1)*dms::DegToRad;
  w = (omega1 - 168.8112)*dms::DegToRad;
  double gamma1rad = (gamma1)*dms::DegToRad;
   X[1] = r1*(cos(u)*cos(w) - sin(u)*cos(gamma1rad)*sin(w));
   Y[1] = r1*(sin(u)*cos(w)*cos(gamma1rad) + cos(u)*sin(w));
   Z[1] = r1*sin(u)*sin(gamma1rad);

  u = (lambda2 - omega2)*dms::DegToRad;
  w = (omega2 - 168.8112)*dms::DegToRad;
   double gamma2rad = (gamma2)*dms::DegToRad;
   X[2] = r2*(cos(u)*cos(w) - sin(u)*cos(gamma2rad)*sin(w));
   Y[2] = r2*(sin(u)*cos(w)*cos(gamma2rad) + cos(u)*sin(w));
   Z[2] = r2*sin(u)*sin(gamma2rad);

  u = (lambda3 - omega3)*dms::DegToRad;
  w = (omega3 - 168.8112)*dms::DegToRad;
   double gamma3rad = (gamma3)*dms::DegToRad;
   X[3] = r3*(cos(u)*cos(w) - sin(u)*cos(gamma3rad)*sin(w));
   Y[3] = r3*(sin(u)*cos(w)*cos(gamma3rad) + cos(u)*sin(w));
   Z[3] = r3*sin(u)*sin(gamma3rad);
 
  u = (lambda4 - omega4)*dms::DegToRad;
  w = (omega4 - 168.8112)*dms::DegToRad;
   double gamma4rad = (gamma4)*dms::DegToRad;
   X[4] = r4*(cos(u)*cos(w) - sin(u)*cos(gamma4rad)*sin(w));
   Y[4] = r4*(sin(u)*cos(w)*cos(gamma4rad) + cos(u)*sin(w));
   Z[4] = r4*sin(u)*sin(gamma4rad);

  u = (lambda5 - omega5)*dms::DegToRad;
  w = (omega5 - 168.8112)*dms::DegToRad;
   double gamma5rad = (gamma5)*dms::DegToRad;
   X[5] = r5*(cos(u)*cos(w) - sin(u)*cos(gamma5rad)*sin(w));
   Y[5] = r5*(sin(u)*cos(w)*cos(gamma5rad) + cos(u)*sin(w));
   Z[5] = r5*sin(u)*sin(gamma5rad);

  u = (lambda6 - omega6)*dms::DegToRad;
  w = (omega6 - 168.8112)*dms::DegToRad;
  double gamma6rad = (gamma6)*dms::DegToRad;
   X[6] = r6*(cos(u)*cos(w) - sin(u)*cos(gamma6rad)*sin(w));
   Y[6] = r6*(sin(u)*cos(w)*cos(gamma6rad) + cos(u)*sin(w));
   Z[6] = r6*sin(u)*sin(gamma6rad); 
 
  u = (lambda7 - omega7)*dms::DegToRad;
  w = (omega7 - 168.8112)*dms::DegToRad;
  double gamma7rad = (gamma7)*dms::DegToRad;
  X[7] = r7*(cos(u)*cos(w) - sin(u)*cos(gamma7rad)*sin(w));
  Y[7] = r7*(sin(u)*cos(w)*cos(gamma7rad) + cos(u)*sin(w));
  Z[7] = r7*sin(u)*sin(gamma7rad);

  u = (lambda8 - omega8)*dms::DegToRad;
  w = (omega8 - 168.8112)*dms::DegToRad;
  double gamma8rad = (gamma8)*dms::DegToRad;
   X[8] = r8*(cos(u)*cos(w) - sin(u)*cos(gamma8rad)*sin(w));
   Y[8] = r8*(sin(u)*cos(w)*cos(gamma8rad) + cos(u)*sin(w));
   Z[8] = r8*sin(u)*sin(gamma8rad);

   X[9] = 0;
   Y[9] = 0;
   Z[9] = 1;

   for ( int i=1; i<10; ++i ) {
   A1[i] = X[i];
   B1[i] = c1*Y[i] - s1*Z[i];
   C1[i] = s1*Y[i] + c1*Z[i];
  
  //Rotation towards the vernal equinox
   A2[i] = c2*A1[i] - s2*B1[i];
   B2[i] = s2*A1[i] + c2*B1[i];
   C2[i] = C1[i];

   A3[i] = A2[i]*sin(LAMBDA) - B2[i]*cos(LAMBDA);
   B3[i] = A2[i]*cos(LAMBDA) + B2[i]*sin(LAMBDA);
   C3[i] = C2[i];

   A4[i] = A3[i];
   B4[i] = B3[i]*cos(ALPHA) + C3[i]*sin(ALPHA);
   C4[i] = C3[i]*cos(ALPHA) - B3[i]*sin(ALPHA);
	}
	
  D = atan2(A4[9], C4[9]);

    //X and Y are now the rectangular coordinates of each satellite,
    //in units of Jupiter's Equatorial radius.
    //When Z is negative, the planet is nearer to the Sun than Jupiter.

    double pa = Saturn->pa()*dms::PI/180.0;

    for ( int i=0; i<8; ++i ) {
        XS[i] = A4[i+1] * cos( D ) - C4[i+1] * sin( D );
        YS[i] = A4[i+1] * sin( D ) + C4[i+1] * cos( D );
        ZS[i] = B4[i+1];

        Moon[i]->setRA( Saturn->ra()->Hours() - 0.011*( XS[i] * cos( pa ) - YS[i] * sin( pa ) )/15.0 );
        Moon[i]->setDec( Saturn->dec()->Degrees() - 0.011*( XS[i] * sin( pa ) + YS[i] * cos( pa ) ) );

        if ( ZS[i] < 0.0 ) InFront[i] = true;
        else InFront[i] = false;

        //Update Trails
        if ( Moon[i]->hasTrail() ) {
            Moon[i]->addToTrail();
            if ( Moon[i]->trail().size() > MAXTRAIL ) Moon[i]->trail().takeFirst();
        }
    }
}
