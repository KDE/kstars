/* Licensed under a 3-clause BSD style license.

    Functions for calculating exact overlap between shapes.

    Original cython version by Thomas Robitaille. Converted to C by Kyle
    Barbary.*/

#pragma once

#include <math.h>

/* Return area of a circle arc between (x1, y1) and (x2, y2) with radius r */
/* reference: http://mathworld.wolfram.com/CircularSegment.html */
static double area_arc(double x1, double y1, double x2, double y2,
			      double r)
{
  double a, theta;

  a = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
  theta = 2. * asin(0.5 * a / r);
  return 0.5 * r * r * (theta - sin(theta));
}

/* Area of a triangle defined by three vertices */
static double area_triangle(double x1, double y1, double x2, double y2,
				   double x3, double y3)
{
  return 0.5 * fabs(x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2));
}

/* Core of circular overlap routine.
 * Assumes that xmax >= xmin >= 0.0, ymax >= ymin >= 0.0.
 * (can always modify input to conform to this).
 */
static double circoverlap_core(double xmin, double ymin,
				      double xmax, double ymax, double r)
{
  double a, b, x1, x2, y1, y2, r2, xmin2, ymin2, xmax2, ymax2;

  xmin2 = xmin*xmin;
  ymin2 = ymin*ymin;
  r2 = r*r;
  if (xmin2 + ymin2 > r2)
    return 0.;

  xmax2 = xmax*xmax;
  ymax2 = ymax*ymax;
  if (xmax2 + ymax2 < r2)
    return (xmax-xmin)*(ymax-ymin);

  a = xmax2 + ymin2;  /* (corner 1 distance)^2 */
  b = xmin2 + ymax2;  /* (corner 2 distance)^2 */

  if (a < r2 && b < r2)
    {
      x1 = sqrt(r2 - ymax2);
      y1 = ymax;
      x2 = xmax;
      y2 = sqrt(r2 - xmax2);
      return ((xmax-xmin)*(ymax-ymin) -
	      area_triangle(x1, y1, x2, y2, xmax, ymax) +
	      area_arc(x1, y1, x2, y2, r));
    }

  if (a < r2)
    {
      x1 = xmin;
      y1 = sqrt(r2 - xmin2);
      x2 = xmax;
      y2 = sqrt(r2 - xmax2);
      return (area_arc(x1, y1, x2, y2, r) +
	      area_triangle(x1, y1, x1, ymin, xmax, ymin) +
	      area_triangle(x1, y1, x2, ymin, x2, y2));
    }

  if (b < r2)
    {
      x1 = sqrt(r2 - ymin2);
      y1 = ymin;
      x2 = sqrt(r2 - ymax2);
      y2 = ymax;
      return (area_arc(x1, y1, x2, y2, r) +
	      area_triangle(x1, y1, xmin, y1, xmin, ymax) +
	      area_triangle(x1, y1, xmin, y2, x2, y2));
    }

  /* else */
  x1 = sqrt(r2 - ymin2);
  y1 = ymin;
  x2 = xmin;
  y2 = sqrt(r2 - xmin2);
  return (area_arc(x1, y1, x2, y2, r) +
	  area_triangle(x1, y1, x2, y2, xmin, ymin));
}



/* Area of overlap of a rectangle and a circle */
static double circoverlap(double xmin, double ymin, double xmax, double ymax,
			  double r)
{
  /* some subroutines demand that r > 0 */
  if (r <= 0.)
    return 0.;

  if (0. <= xmin)
    {
      if (0. <= ymin)
	return circoverlap_core(xmin, ymin, xmax, ymax, r);
      else if (0. >= ymax)
	return circoverlap_core(-ymax, xmin, -ymin, xmax, r);
      else
	return (circoverlap(xmin, ymin, xmax, 0., r) +
                circoverlap(xmin, 0., xmax, ymax, r));
    }
  else if (0. >= xmax)
    {
      if (0. <= ymin)
	return circoverlap_core(-xmax, ymin, -xmin, ymax, r);
      else if (0. >= ymax)
	return circoverlap_core(-xmax, -ymax, -xmin, -ymin, r);
      else
	return (circoverlap(xmin, ymin, xmax, 0., r) +
                circoverlap(xmin, 0., xmax, ymax, r));
    }
  else
    {
      if (0. <= ymin)
	return (circoverlap(xmin, ymin, 0., ymax, r) +
                circoverlap(0., ymin, xmax, ymax, r));
      if (0. >= ymax)
	return (circoverlap(xmin, ymin, 0., ymax, r) +
                circoverlap(0., ymin, xmax, ymax, r));
      else
	return (circoverlap(xmin, ymin, 0., 0., r) +
                circoverlap(0., ymin, xmax, 0., r) +
                circoverlap(xmin, 0., 0., ymax, r) +
                circoverlap(0., 0., xmax, ymax, r));
    }
}

/*
Start of new circular overlap routine that might be faster.

double circoverlap_new(double dx, double dy, double r)
{
  double xmin, xmax, ymin, ymax, xmin2, xmax2, ymin2, ymax2, r2;

  if (dx < 0.)
    dx = -dx;
  if (dy < 0.)
    dy = -dy;
  if (dy > dx)
    {
      r2 = dy;
      dy = dx;
      dx = r2;
    }

  xmax = dx + 0.5;
  ymax = dy + 0.5;
  xmax2 = xmax*xmax;
  ymax2 = ymax*ymax;
  r2 = r*r;

  if (xmax2 + ymax2 < r2)
    return 1.;

  xmin2 = xmin*xmin;
  if (xmin2 +
}
*/

/*****************************************************************************/
/* ellipse overlap functions */

typedef struct
{
  double x, y;
} point;


typedef struct
{
  point p1, p2;
} intersections;

static void swap(double *a, double *b)
{
  double temp;
  temp = *a;
  *a = *b;
  *b = temp;
}

static void swap_point(point *a, point *b)
{
  point temp;
  temp = *a;
  *a = *b;
  *b = temp;
}

/* rotate values to the left: a=b, b=c, c=a */
static void rotate(double *a, double *b, double *c)
{
  double temp;
  temp = *a;
  *a = *b;
  *b = *c;
  *c = temp;
}

/* Check if a point (x,y) is inside a triangle */
static int in_triangle(double x, double y, double x1, double y1,
		       double x2, double y2, double x3, double y3)
{
  int c;

  c = 0;
  c += ((y1 > y) != (y2 > y)) && (x < (x2-x1) * (y-y1) / (y2-y1) + x1);
  c += ((y2 > y) != (y3 > y)) && (x < (x3-x2) * (y-y2) / (y3-y2) + x2);
  c += ((y3 > y) != (y1 > y)) && (x < (x1-x3) * (y-y3) / (y1-y3) + x3);

  return c % 2 == 1;
}


/* Intersection of a line defined by two points with a unit circle */
static intersections circle_line(double x1, double y1, double x2, double y2)
{
  double a, b, delta, dx, dy;
  double tol = 1.e-10;
  intersections inter;

  dx = x2 - x1;
  dy = y2 - y1;

  if (fabs(dx) < tol && fabs(dy) < tol)
    {
      inter.p1.x = 2.;
      inter.p1.y = 2.;
      inter.p2.x = 2.;
      inter.p2.y = 2.;
    }
  else if (fabs(dx) > fabs(dy))
    {
      /* Find the slope and intercept of the line */
      a = dy / dx;
      b = y1 - a * x1;

      /* Find the determinant of the quadratic equation */
      delta = 1. + a*a - b*b;
      if (delta > 0.)  /* solutions exist */
	{
	  delta = sqrt(delta);

	  inter.p1.x = (-a*b - delta) / (1. + a*a);
	  inter.p1.y = a * inter.p1.x + b;
	  inter.p2.x = (-a*b + delta) / (1. + a*a);
	  inter.p2.y = a * inter.p2.x + b;
	}
      else  /* no solution, return values > 1 */
	{
	  inter.p1.x = 2.;
	  inter.p1.y = 2.;
	  inter.p2.x = 2.;
	  inter.p2.y = 2.;
	}
    }
  else
    {
      /* Find the slope and intercept of the line */
      a = dx / dy;
      b = x1 - a * y1;

      /* Find the determinant of the quadratic equation */
      delta = 1. + a*a - b*b;
      if (delta > 0.)  /* solutions exist */
	{
	  delta = sqrt(delta);
	  inter.p1.y = (-a*b - delta) / (1. + a*a);
	  inter.p1.x = a * inter.p1.y + b;
	  inter.p2.y = (-a*b + delta) / (1. + a*a);
	  inter.p2.x = a * inter.p2.y + b;
	}
      else  /* no solution, return values > 1 */
	{
	  inter.p1.x = 2.;
	  inter.p1.y = 2.;
	  inter.p2.x = 2.;
	  inter.p2.y = 2.;
	}
    }

  return inter;
}

/* The intersection of a line with the unit circle. The intersection the
 * closest to (x2, y2) is chosen. */
static point circle_segment_single2(double x1, double y1, double x2, double y2)
{
  double dx1, dy1, dx2, dy2;
  intersections inter;
  point pt1, pt2, pt;

  inter = circle_line(x1, y1, x2, y2);
  pt1 = inter.p1;
  pt2 = inter.p2;

  /*Can be optimized, but just checking for correctness right now */
  dx1 = fabs(pt1.x - x2);
  dy1 = fabs(pt1.y - y2);
  dx2 = fabs(pt2.x - x2);
  dy2 = fabs(pt2.y - y2);

  if (dx1 > dy1)  /* compare based on x-axis */
    pt = (dx1 > dx2) ? pt2 : pt1;
  else
    pt = (dy1 > dy2) ? pt2 : pt1;
     
  return pt;
}

/* Intersection(s) of a segment with the unit circle. Discard any
   solution not on the segment. */
intersections circle_segment(double x1, double y1, double x2, double y2)
{
  intersections inter, inter_new;
  point pt1, pt2;

  inter = circle_line(x1, y1, x2, y2);
  pt1 = inter.p1;
  pt2 = inter.p2;

  if ((pt1.x > x1 && pt1.x > x2) || (pt1.x < x1 && pt1.x < x2) ||
      (pt1.y > y1 && pt1.y > y2) || (pt1.y < y1 && pt1.y < y2))
    {
      pt1.x = 2.;
      pt1.y = 2.;
    }
  if ((pt2.x > x1 && pt2.x > x2) || (pt2.x < x1 && pt2.x < x2) ||
      (pt2.y > y1 && pt2.y > y2) || (pt2.y < y1 && pt2.y < y2))
    {
      pt2.x = 2.;
      pt2.y = 2.;
    }

  if (pt1.x > 1. && pt2.x < 2.)
    {
      inter_new.p1 = pt1;
      inter_new.p2 = pt2;
    }
  else
    {
      inter_new.p1 = pt2;
      inter_new.p2 = pt1;
    }
  return inter_new;
}


/* Given a triangle defined by three points (x1, y1), (x2, y2), and
   (x3, y3), find the area of overlap with the unit circle. */
static double triangle_unitcircle_overlap(double x1, double y1,
					  double x2, double y2,
					  double x3, double y3)
{
  double d1, d2, d3, area, xp, yp;
  int in1, in2, in3, on1, on2, on3;
  int intersect13, intersect23;
  intersections inter;
  point pt1, pt2, pt3, pt4, pt5, pt6;

  /* Find distance of all vertices to circle center */
  d1 = x1*x1 + y1*y1;
  d2 = x2*x2 + y2*y2;
  d3 = x3*x3 + y3*y3;

  /* Order vertices by distance from origin */
  if (d1 < d2)
    {
      if (d2 < d3)
	{}
      else if (d1 < d3)
	{
	  swap(&x2, &x3);
	  swap(&y2, &y3);
	  swap(&d2, &d3);
	}
      else
	{
	  rotate(&x1, &x3, &x2);
	  rotate(&y1, &y3, &y2);
	  rotate(&d1, &d3, &d2);
	}
    }
  else
    {
      if (d1 < d3)
	{
	  swap(&x1, &x2);
	  swap(&y1, &y2);
	  swap(&d1, &d2);
	}
      else if (d2 < d3)
	{
	  rotate(&x1, &x2, &x3);
	  rotate(&y1, &y2, &y3);
	  rotate(&d1, &d2, &d3);
	}
      else
	{
	  swap(&x1, &x3);
	  swap(&y1, &y3);
	  swap(&d1, &d3);
	}
    }
     
  /* Determine number of vertices inside circle */
  in1 = d1 < 1.;
  in2 = d2 < 1.;
  in3 = d3 < 1.;

  /* Determine which vertices are on the circle */
  on1 = fabs(d1 - 1.) < 1.e-10;
  on2 = fabs(d2 - 1.) < 1.e-10;
  on3 = fabs(d3 - 1.) < 1.e-10;

  if (on3 || in3) /* triangle completely within circle */
    area = area_triangle(x1, y1, x2, y2, x3, y3);

  else if (in2 || on2)
    {
      /* If vertex 1 or 2 are on the edge of the circle, then we use
       * the dot product to vertex 3 to determine whether an
       * intersection takes place. */
      intersect13 = !on1 || (x1*(x3-x1) + y1*(y3-y1) < 0.);
      intersect23 = !on2 || (x2*(x3-x2) + y2*(y3-y2) < 0.);
      if (intersect13 && intersect23)
	{
	  pt1 = circle_segment_single2(x1, y1, x3, y3);
	  pt2 = circle_segment_single2(x2, y2, x3, y3);
	  area = (area_triangle(x1, y1, x2, y2, pt1.x, pt1.y) +
		  area_triangle(x2, y2, pt1.x, pt1.y, pt2.x, pt2.y) +
		  area_arc(pt1.x, pt1.y, pt2.x, pt2.y, 1.));
	}
      else if (intersect13)
	{
	  pt1 = circle_segment_single2(x1, y1, x3, y3);
	  area = (area_triangle(x1, y1, x2, y2, pt1.x, pt1.y) +
		  area_arc(x2, y2, pt1.x, pt1.y, 1.));
	}
      else if (intersect23)
	{
	  pt2 = circle_segment_single2(x2, y2, x3, y3);
	  area = (area_triangle(x1, y1, x2, y2, pt2.x, pt2.y) +
		  area_arc(x1, y1, pt2.x, pt2.y, 1.));
	}
      else
	area = area_arc(x1, y1, x2, y2, 1.);
    }
  else if (in1)
    {
      /* Check for intersections of far side with circle */
      inter = circle_segment(x2, y2, x3, y3);
      pt1 = inter.p1;
      pt2 = inter.p2;
      pt3 = circle_segment_single2(x1, y1, x2, y2);
      pt4 = circle_segment_single2(x1, y1, x3, y3);

      if (pt1.x > 1.)  /* indicates no intersection */
	{
	  /* check if the pixel vertex (x1, y2) and the origin are on
	   * different sides of the circle segment. If they are, the
	   * circle segment spans more than pi radians.
	   * We use the formula (y-y1) * (x2-x1) > (y2-y1) * (x-x1)
	   * to determine if (x, y) is on the left of the directed
	   * line segment from (x1, y1) to (x2, y2) */
	  if (((0.-pt3.y) * (pt4.x-pt3.x) > (pt4.y-pt3.y) * (0.-pt3.x)) !=
	      ((y1-pt3.y) * (pt4.x-pt3.x) > (pt4.y-pt3.y) * (x1-pt3.x)))
	    {
	      area = (area_triangle(x1, y1, pt3.x, pt3.y, pt4.x, pt4.y) +
		      PI - area_arc(pt3.x, pt3.y, pt4.x, pt4.y, 1.));
	    }
	  else
	    {
	      area = (area_triangle(x1, y1, pt3.x, pt3.y, pt4.x, pt4.y) +
		      area_arc(pt3.x, pt3.y, pt4.x, pt4.y, 1.));
	    }
	}
      else
	{
	  /* ensure that pt1 is the point closest to (x2, y2) */
	  if (((pt2.x-x2)*(pt2.x-x2) + (pt2.y-y2)*(pt2.y-y2)) <
	      ((pt1.x-x2)*(pt1.x-x2) + (pt1.y-y2)*(pt1.y-y2)))
	    swap_point(&pt1, &pt2);

	  area = (area_triangle(x1, y1, pt3.x, pt3.y, pt1.x, pt1.y) +
		  area_triangle(x1, y1, pt1.x, pt1.y, pt2.x, pt2.y) +
		  area_triangle(x1, y1, pt2.x, pt2.y, pt4.x, pt4.y) +
		  area_arc(pt1.x, pt1.y, pt3.x, pt3.y, 1.) +
		  area_arc(pt2.x, pt2.y, pt4.x, pt4.y, 1.));
	}
    }
  else
    {
      inter = circle_segment(x1, y1, x2, y2);
      pt1 = inter.p1;
      pt2 = inter.p2;
      inter = circle_segment(x2, y2, x3, y3);
      pt3 = inter.p1;
      pt4 = inter.p2;
      inter = circle_segment(x3, y3, x1, y1);
      pt5 = inter.p1;
      pt6 = inter.p2;
      if (pt1.x <= 1.)
	{
	  xp = 0.5 * (pt1.x + pt2.x);
	  yp = 0.5 * (pt1.y + pt2.y);
	  area = (triangle_unitcircle_overlap(x1, y1, x3, y3, xp, yp) +
		  triangle_unitcircle_overlap(x2, y2, x3, y3, xp, yp));
	}
      else if (pt3.x <= 1.)
	{
	  xp = 0.5 * (pt3.x + pt4.x);
	  yp = 0.5 * (pt3.y + pt4.y);
	  area = (triangle_unitcircle_overlap(x3, y3, x1, y1, xp, yp) +
		  triangle_unitcircle_overlap(x2, y2, x1, y1, xp, yp));
	}
      else if (pt5.x <= 1.)
	{
	  xp = 0.5 * (pt5.x + pt6.x);
	  yp = 0.5 * (pt5.y + pt6.y);
	  area = (triangle_unitcircle_overlap(x1, y1, x2, y2, xp, yp) +
		  triangle_unitcircle_overlap(x3, y3, x2, y2, xp, yp));
	}
      else  /* no intersections */
	{
	  if (in_triangle(0., 0., x1, y1, x2, y2, x3, y3))
	    return PI;
	  else
	    return 0.;
	}
    }
  return area;
}

/* exact overlap between a rectangle defined by (xmin, ymin, xmax,
   ymax) and an ellipse with major and minor axes rx and ry
   respectively and position angle theta. */
static double ellipoverlap(double xmin, double ymin, double xmax, double ymax,
			   double a, double b, double theta)
{
  double cos_m_theta, sin_m_theta, scale;
  double x1, y1, x2, y2, x3, y3, x4, y4;

  cos_m_theta = cos(-theta);
  sin_m_theta = sin(-theta);

  /* scale by which the areas will be shrunk */
  scale = a * b;

  /* Reproject rectangle to a frame in which ellipse is a unit circle */
  x1 = (xmin * cos_m_theta - ymin * sin_m_theta) / a;
  y1 = (xmin * sin_m_theta + ymin * cos_m_theta) / b;
  x2 = (xmax * cos_m_theta - ymin * sin_m_theta) / a;
  y2 = (xmax * sin_m_theta + ymin * cos_m_theta) / b;
  x3 = (xmax * cos_m_theta - ymax * sin_m_theta) / a;
  y3 = (xmax * sin_m_theta + ymax * cos_m_theta) / b;
  x4 = (xmin * cos_m_theta - ymax * sin_m_theta) / a;
  y4 = (xmin * sin_m_theta + ymax * cos_m_theta) / b;

  /* Divide resulting quadrilateral into two triangles and find
     intersection with unit circle */
  return scale * (triangle_unitcircle_overlap(x1, y1, x2, y2, x3, y3) +
		  triangle_unitcircle_overlap(x1, y1, x4, y4, x3, y3));
}
