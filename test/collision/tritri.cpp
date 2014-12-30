//$c1   AKP 08/29/06 Created 
//========================================================================//
//              Copyright 2006 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	tritri.cpp
//     
//     Application:	Triangle Triangle Intersection
//     
//     Contents: Utility to detect intersection between two 3D triangles
//
//========================================================================//

//BEGIN PCH includes
#include "stdafx.h"
//END PCH includes
#include "tritri.h"

const double EPSILON = 1e-10;

// cross-product
inline void cross( double dest[3], double v1[3], double v2[3] )
{
	dest[0] = v1[1]*v2[2] - v1[2]*v2[1];
	dest[1] = v1[2]*v2[0] - v1[0]*v2[2];
	dest[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

// dot-product
inline double dot( double v1[3], double v2[3] )
{
	return( v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2] );
}

// 2D dot-product
inline double dot2( double v1[2], double v2[2] )
{
	return( v1[0]*v2[0] + v1[1]*v2[1] );
}

// subtract
inline void subtract( double dest[3], double v1[3], double v2[3] )
{
	dest[0] = v1[0] - v2[0];
    dest[1] = v1[1] - v2[1];
    dest[2] = v1[2] - v2[2]; 
}

inline void plane_normal( double dest[3], double v0[3], double v1[3], double v2[3] )
{
	double v01[3];
	subtract( v01, v1, v0 );

	double v02[3];
	subtract( v02, v2, v0 );

	cross( dest, v01, v02 );
}

inline double plane_equation( double dest[3], double v0[3], double v1[3], double v2[3] )
{
	plane_normal( dest, v0, v1, v2 );
	double dist = dot( dest, v0 );

	return -dist; // X.N + d = 0;
}

inline double plane_distance( double v[3], double n[3], double d )
{
	double dist = dot(v,n) + d;
	// clamp the dist to 0.0 if the vertex is near-coplanar to the plane
	if( std::fabs(dist) < EPSILON )
		dist = 0.0;

	return dist;
}

inline bool same_sign( double a, double b )
{
	//return (a*b > 0.0);
	return (a*b >= 0.0); // AKP: this is wrong but correct for our requirement
}

inline bool same_sign( double a, double b, double c )
{
	return same_sign(a,b) && same_sign(a,c);
}

static void project_onto_axis(
					double vp[3], double up[3], double d[3],
					double v0[3], double v1[3], double v2[3],
					double u0[3], double u1[3], double u2[3] )
{
	// instead of calculating dot product,
	// just find the coordinate axis with which d is most closely aligned
	// and return the corresponding coordinates of vertices

	unsigned int maxi = 0;
	double maxv = std::fabs( d[0] );
	double y = std::fabs( d[1] );
	if( y > maxv )
	{
		maxv = y;
		maxi = 1;
	}
	double z = std::fabs( d[2] );
	if( z > maxv ) maxi = 2;

	vp[0] = v0[maxi];
	vp[1] = v1[maxi];
	vp[2] = v2[maxi];

	up[0] = u0[maxi];
	up[1] = u1[maxi];
	up[2] = u2[maxi];
}

inline void sort( double a[2] )
{
	if( a[0] > a[1] )
		std::swap( a[0], a[1] );
}

inline void swap( double v[3], double w[3] )
{
	for (int i=0; i<3; i++)
		std::swap( v[i], w[i] );
}

inline void isect( double vp0, double vp1, double vp2, double dv0, double dv1, double dv2, double inter[2] )
{
	// v1 and v2 are on the same side, v0 is on the opposite side
	inter[0] = vp0 + (vp1-vp0)*dv0/(dv0-dv1);
    inter[1] = vp0 + (vp2-vp0)*dv0/(dv0-dv2);
	sort( inter );
}

static void compute_interval( double vp0, double vp1, double vp2, double dv0, double dv1, double dv2, double inter[2] )
{
	if( same_sign(dv0,dv1) )
	{
		// v0, v1 are on the same side, v2 on the other or on the plane
		isect( vp2, vp0, vp1, dv2, dv0, dv1, inter );
	}
	else if( same_sign(dv0,dv2) )
	{
		// v0, v2 are on the same side, v1 on the other or on the plane
		isect( vp1, vp0, vp2, dv1, dv0, dv2, inter );
	}
	else if( same_sign(dv1,dv2) || (dv0 != 0.0) )
	{
		// v1, v2 are on the same side or v0 is not on the plane
		isect( vp0, vp1, vp2, dv0, dv1, dv2, inter );
	}
	else if( dv1 != 0.0 )
	{
		// v1 is not on the plane, v0 and v2 may be on the plane
		isect( vp1, vp0, vp2, dv1, dv0, dv2, inter );
	}
	else if( dv2 != 0.0 )
	{
		// v2 is not on the plane, v0 and v1 may be on the plane
		isect( vp2, vp0, vp1, dv2, dv0, dv1, inter );
	}
	else
	{
		// should not come here, this is a coplanar case which has been handled earlier
		//SU_ASSERT( AKP, 0 );
	}
}

// test if intervals a and b overlap
inline bool overlap( double a[2], double b[2] )
{
	if( (a[1] < b[0]) || (b[1] < a[0]) )
		return false;
	else
		return true;
}

// test if edge v0, v1 separates tri0 and tri1
inline bool separates(
				double v0[2], double v1[2], double v2[2],
				double u0[2], double u1[2], double u2[2] )
{
	// calculate edge
	double e[2] = { v1[0]-v0[0], v1[1]-v0[1] };
	double n[2] = { -e[1], e[0] }; // perpendicular to edge

	// calculate the interval of tri0
	double inter0[2] = { dot2(n,v0), dot2(n,v2) };
	sort( inter0 );

	// calculate the interval of tri0
	// vertex u0
	double dotp = dot2( n, u0 );
	double inter1[2] = { dotp, dotp };
	// vertex u1
	dotp = dot2( n, u1 );
	if( dotp < inter1[0] )
		inter1[0] = dotp;
	else if( dotp > inter1[1] )
		inter1[1] = dotp;
	// vertex u2
	dotp = dot2( n, u2 );
	if( dotp < inter1[0] )
		inter1[0] = dotp;
	else if( dotp > inter1[1] )
		inter1[1] = dotp;

	// if the two intervals overlap, the edge does not separate tri0 and tri1
	return overlap(inter0, inter1) ? false : true;
}


inline bool moDeterminateSign3D(double a[3], double b[3], double c[3], double d[3], bool *isZero)
{
	double v0[3], v1[3], v2[3], temp[3], value;

	if (isZero)
		*isZero = false;

	for (int i=0; i<3; i++)
	{
		v0[i] = a[i] - d[i];
		v1[i] = b[i] - d[i];
		v2[i] = c[i] - d[i];
	}
	
	cross(temp, v0, v1);
	value = dot(temp, v2);
	if (value > EPSILON)
		return true;

	if (isZero && value > -EPSILON)
		*isZero = true;

	return false;
}

inline bool moDeterminateSign2D(double a[3], double b[3], double c[3], double normal[3], bool *isZero)
{
	double d[3];
	for (int i=0; i<3; i++)
		d[i] = a[i] + normal[i];

	return moDeterminateSign3D(a, b, c, d, isZero);
}

// moTriTriIntersect
////////////////////////////////////////////////////////////////////////////////////
bool moTriTriIntersectMueller(
				double v0[3], double v1[3], double v2[3],
				double u0[3], double u1[3], double u2[3] )
{
	// calculate plane of tri1
	double n1[3];
	double d1 = plane_equation( n1, v0, v1, v2 );
	// put the vertices of tri2 into the plane equation of tri1 to compute signed distances to the plane
	double du0 = plane_distance( u0, n1, d1 );
	double du1 = plane_distance( u1, n1, d1 );
	double du2 = plane_distance( u2, n1, d1 );
	// if all the distances are of the same sign => all the vertices of tri2 are on the same side of tri1
	// hence they cannot intersect
	if( same_sign(du0, du1, du2) )
		return false;

	// calculate plane of tri2
	double n2[3];
	double d2 = plane_equation( n2, u0, u1, u2 );
	// put the vertices of tri2 into the plane equation of tri1 to compute signed distances to the plane
	double dv0 = plane_distance( v0, n2, d2 );
	double dv1 = plane_distance( v1, n2, d2 );
	double dv2 = plane_distance( v2, n2, d2 );
	// if all the distances are of the same sign => all the vertices of tri2 are on the same side of tri1
	// hence they cannot intersect
	if( same_sign(dv0, dv1, dv2) )
		return false;

	// compute direction of intersection line
	// this intersection line intersects with both triangles
	double d[3];
	cross( d, n1, n2 );

	// handle coplanar case
	//if( dot(d,d) < EPSILON )
	//	return moTriTriIntersectCoplanar( v0, v1, v2, u0, u1, u2 );

	// project the triangle coordinates onto the intersection line
	double vp[3], up[3];
	project_onto_axis( vp, up, d, v0, v1, v2, u0, u1, u2 );

	// compute the intervals of intersection
	double inter1[2], inter2[2];
	compute_interval( vp[0], vp[1], vp[2], dv0, dv1, dv2, inter1 );
	compute_interval( up[0], up[1], up[2], du0, du1, du2, inter2 );
	return overlap( inter1, inter2 );
}

// moTriTriIntersectCoplanar
////////////////////////////////////////////////////////////////////////////////////
bool moTriTriIntersectCoplanar(
				double v0[3], double v1[3], double v2[3],
				double u0[3], double u1[3], double u2[3] )
{
	// project the triangles onto a plane that maximizes their area
	double n[3];
	plane_normal( n, v0, v1, v2 );
	n[0] = std::fabs( n[0] );
	n[1] = std::fabs( n[1] );
	n[2] = std::fabs( n[2] );

	unsigned int i0, i1;
	if( n[0] > n[1] )
	{
		if( n[0] > n[2] ) // n[0] is greatest
		{
			i0 = 1; i1 = 2;
		}
		else // n[2] is greatest
		{
			i0 = 0; i1 = 1;
		}
	}
	else // n[0] <= n[1]
	{
		if( n[2] > n[1] ) // n[2] is greatest
		{
			i0 = 0; i1 = 1;
		}
		else // n[1] is greatest
		{
			i0 = 0; i1 = 2;
		}
	}

	v0[0] = v0[i0]; v0[1] = v0[i1];
	v1[0] = v1[i0]; v1[1] = v1[i1];
	v2[0] = v2[i0]; v2[1] = v2[i1];

	u0[0] = u0[i0]; u0[1] = u0[i1];
	u1[0] = u1[i0]; u1[1] = u1[i1];
	u2[0] = u2[i0]; u2[1] = u2[i1];

	return moTriTriIntersect2D( v0, v1, v2, u0, u1, u2 );
}

// moTriTriIntersect2D
////////////////////////////////////////////////////////////////////////////////////
bool moTriTriIntersect2D(
				double v0[2], double v1[2], double v2[2],
				double u0[2], double u1[2], double u2[2] )
{
	// test the edges of tri0 for separation
	if( separates(v0, v1, v2, u0, u1, u2) )
		return false;
	if( separates(v1, v2, v0, u0, u1, u2) )
		return false;
	if( separates(v2, v0, v1, u0, u1, u2) )
		return false;

	// test the edges of tri1 for separation
	if( separates(u0, u1, u2, v0, v1, v2) )
		return false;
	if( separates(u1, u2, u0, v0, v1, v2) )
		return false;
	if( separates(u2, u0, u1, v0, v1, v2) )
		return false;

	return true;
}

bool moTriTriIntersectDevillers2D(
				double v0[3], double v1[3], double v2[3],
				double u0[3], double u1[3], double u2[3] )
{
	// Compute normal and project onto second tri onto plane of first.
	return false;

}


bool moTriTriIntersectDevillers(
				double v0[3], double v1[3], double v2[3],
				double u0[3], double u1[3], double u2[3], bool ignore2D )
{

	// Begin like Moeller, look for intersection of tri with the plane of the other tri. 
	// Note: we use the zero check to test for co-planar condition
	// Also: this does not consider degenerate tri's (i.e., points are assumed to be distinct).

	bool isZero[3];
	bool t0 = moDeterminateSign3D( v0, v1, v2, u0, &(isZero[0]) );
	bool t1 = moDeterminateSign3D( v0, v1, v2, u1, &(isZero[1]) );
	bool t2 = moDeterminateSign3D( v0, v1, v2, u2, &(isZero[2]) );

	if ( isZero[0] && isZero[1] && isZero[2] )
	{
		if (ignore2D)
			return false;
		else
			return moTriTriIntersectCoplanar( v0, v1, v2, u0, u1, u2 );
	}
		//return moTriTriIntersectDevillers2D( v0, v1, v2, u0, u1, u2 );

	if ( t0 == t1 && t0 == t2 )
		return false;

	// Put the different sign in the first position
	if (t0 == t1)
	{
		if (isZero[2])
			return false;

		swap(u0,u2);
		t0 = t2;
	}
	else if (t0 == t2)
	{
		if (isZero[1])
			return false;

		swap(u0,u1);
		t0 = t1;
	}
	else if (isZero[0])
		return false;


	bool t3 = moDeterminateSign3D( u0, u1, u2, v0, &(isZero[0]) );
	bool t4 = moDeterminateSign3D( u0, u1, u2, v1, &(isZero[1]) );
	bool t5 = moDeterminateSign3D( u0, u1, u2, v2, &(isZero[2]) );

	if ( t3 == t4 && t3 == t5 )
		return false;


	if (t3 == t4)
	{
		if (isZero[2])
			return false;
		isZero[2] = isZero[0];
		swap(v0,v2);
		t3 = t5;
	}
	else if (t3 == t5)
	{
		if (isZero[1])
			return false;

		isZero[1] = isZero[0];
		swap(v0,v1);
		t3 = t4;
	}
	else if (isZero[0])
		return false;

	if (isZero[1] && isZero[2])
		return false;

	// Make that sign positive. To change signed, swap two elements in other tri.
	// if (t4 == t5) we did not swap second Tri, so t0 better be positive OR
	// we did swap (t4 != t5) and t0 was negative. Otherwise, swap again... 
	if ( t0 == (t4 != t5) )
		swap(v1, v2);

	if ( t3 == false )
		swap(u1, u2);

	//So, the first position is positive and the other positions are negative. 
	//We can now consider, the triple product (signed determinant) using screw theory.
	// Basically, see the paper for this....

	if (moDeterminateSign3D(v0, v1, u0, u1, &(isZero[0])) || isZero[0])
		return false;

	if (moDeterminateSign3D(v0, v2, u2, u0, &(isZero[1])) || isZero[1])
		return false;

	return true;
}

static bool useMueller = false;
static bool debugTriTri = false;
bool moTriTriIntersect(
				double v0[3], double v1[3], double v2[3],
				double u0[3], double u1[3], double u2[3], bool ignore2D )
{
	if (useMueller)
		return moTriTriIntersectMueller( v0, v1, v2, u0, u1, u2 );

	bool returnValue = moTriTriIntersectDevillers( v0, v1, v2, u0, u1, u2, ignore2D );
	if (debugTriTri)
	{
		bool testValue = moTriTriIntersectMueller( v0, v1, v2, u0, u1, u2 );
//		SU_ASSERT(BAD_TRI_TRI, testValue == returnValue);
	}

	return returnValue;
}
	
