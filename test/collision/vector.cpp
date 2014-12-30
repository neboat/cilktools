//========================================================================//
//              Copyright 1994 (Unpublished Material)                     //
//                  SolidWorks Inc.										  //
//========================================================================//
//
//     File Name: 	vector.cpp
//     
//     Application: Math/Geometry utilities      
//     
//     Contents: methods for classes for simple 3d geometry elements
//
//========================================================================//
// Include this set first to get PCH efficiency
#define MATHGEOM_FILE
#include "stdafx.h"
// end pch efficiency set

static const mgVector_c NullVec( 0, 0, 0 );

mgVector_c::mgVector_c( double x, double y, double z )
{
	iComp[0] = x;
	iComp[1] = y;
	iComp[2] = z;
}

mgVector_c::mgVector_c( const double v[ 3 ] )
{ 
	iComp[0] = v[0];
	iComp[1] = v[1];
	iComp[2] = v[2];
}

mgVector_c::mgVector_c( )
{ 
	iComp[0] = 0.0;
	iComp[1] = 0.0;
	iComp[2] = 0.0;
}

mgVector_c::mgVector_c( const mgVector_c& v )
{
	iComp[0] = (v.iComp)[0];
	iComp[1] = (v.iComp)[1];
	iComp[2] = (v.iComp)[2];
}


mgVector_c const&	 mgVector_c::operator=(const mgVector_c& v)
{
	iComp[0] = (v.iComp)[0];
	iComp[1] = (v.iComp)[1];
	iComp[2] = (v.iComp)[2];
	return *this; 
}

mgVector_c::~mgVector_c() 
{ 
}

void mgVector_c::set_x( double new_x ) 
{ 
	iComp[0] = new_x;
}

void mgVector_c::set_y( double new_y ) 
{ 
	iComp[1] = new_y;
}

void mgVector_c::set_z( double new_z ) 
{ 
	iComp[2] = new_z;
}

void mgVector_c::set( double compIn[3] ) 
{
	memcpy( iComp, compIn, 3 * sizeof( double ) );   
}

void mgVector_c::set( double new_x, double new_y, double new_z)
{
	iComp[0] = new_x;
	iComp[1] = new_y;
	iComp[2] = new_z;
}
	 
 mgVector_c operator-( mgVector_c const &v ) 
{ 
	return mgVector_c ( -(v.iComp[0]), -(v.iComp[1]), -(v.iComp[2]) );
}

 mgVector_c operator+( mgVector_c const &v1, mgVector_c const &v2 ) 
{ 
	return mgVector_c	(  ( v1.iComp[0] + v2.iComp[0] ),
								( v1.iComp[1] + v2.iComp[1] ),
								( v1.iComp[2] + v2.iComp[2] )
							);	
}

mgVector_c const &mgVector_c::operator+=( mgVector_c const &v ) 
{ 
	iComp[0] += (v.iComp)[0];
	iComp[1] += (v.iComp)[1];
	iComp[2] += (v.iComp)[2];

	return *this; 
}

 mgVector_c operator-( mgVector_c const &v1, mgVector_c const &v2 ) 
{ 
	return mgVector_c	(   ( v1.iComp[0] - v2.iComp[0] ),
								( v1.iComp[1] - v2.iComp[1] ),
								( v1.iComp[2] - v2.iComp[2] )
							);	
}

 mgVector_c operator*( double s, mgVector_c const &v ) 
{ 
	return mgVector_c (	s * ( v.iComp[0] ),	s * ( v.iComp[1] ), s * ( v.iComp[2] ) );						
}

 mgVector_c operator*( mgVector_c const &v, double s ) 
{ 
	return mgVector_c (	s * ( v.iComp[0] ),	s * ( v.iComp[1] ), s * ( v.iComp[2] ) );
}

mgVector_c const &mgVector_c::operator*=( double s ) 
{ 
	iComp[0] *= s;
	iComp[1] *= s;
	iComp[2] *= s;

	return *this; 
}

 double operator%( mgVector_c const &v1, mgVector_c const &v2 )
{
	//	Dot product
	return	( 
				( v1.iComp[0] * v2.iComp[0] ) +
						( v1.iComp[1] * v2.iComp[1] ) +
								( v1.iComp[2] * v2.iComp[2] )
			); 
}


 mgVector_c operator*( mgVector_c const &v1, mgVector_c const &v2 ) 
{ 
	//	Cross product
	double	components[3];
	components[0] = ( (v1.iComp )[1] * (v2.iComp )[2] ) -
								( (v1.iComp )[2] * (v2.iComp )[1] );

	components[1] = ( (v1.iComp )[2] * (v2.iComp )[0] ) -
								( (v1.iComp )[0] * (v2.iComp )[2] );

	components[2] = ( (v1.iComp )[0] * (v2.iComp )[1] ) -
								( (v1.iComp )[1] * (v2.iComp )[0] ); 

	return mgVector_c ( components );	 
}



 mgVector_c operator/( mgVector_c const &v, double s) 
{
	if ( fabs(s) < gcLengthTolerance )
		return	mgVector_c( 0.0, 0.0, 0.0 );
	else
		return mgVector_c ( ( v.iComp[0] ) / s, ( v.iComp[1] ) / s, ( v.iComp[2] ) / s );		
}



BOOL mgVector_c::isNotNull() const
{
if ( fabs ( iComp[0] ) <gcLengthTolerance &&  fabs ( iComp[1] ) <gcLengthTolerance	&&  fabs ( iComp[2] ) < gcLengthTolerance  )
	{
	return FALSE;
	}
return TRUE;
}

BOOL mgVector_c::isNull() const
{
if ( fabs ( iComp[0] ) <gcLengthTolerance &&  fabs ( iComp[1] ) <gcLengthTolerance	&&  fabs ( iComp[2] ) < gcLengthTolerance  )
	{
	return TRUE;
	}
return FALSE;
}

 BOOL operator==(mgVector_c const &v1, mgVector_c const &v2)
{
	return (
				( fabs ( (v1.iComp[0] ) - ( v2.iComp[0] ) ) < gcLengthTolerance ) &&
				( fabs ( (v1.iComp[1] ) - ( v2.iComp[1] ) ) < gcLengthTolerance ) &&
				( fabs ( (v1.iComp[2] ) - ( v2.iComp[2] ) ) < gcLengthTolerance ) 
			);
}

 BOOL operator!=(mgVector_c const &v1, mgVector_c const &v2)
{
	return (
				( fabs ( (v1.iComp[0] ) - ( v2.iComp[0] ) ) > gcLengthTolerance ) ||
				( fabs ( (v1.iComp[1] ) - ( v2.iComp[1] ) ) > gcLengthTolerance ) ||
				( fabs ( (v1.iComp[2] ) - ( v2.iComp[2] ) ) > gcLengthTolerance ) 
			);
}

 mgVector_c mg_Normalise( mgVector_c const &v )
{
	double denominator = v.x()*v.x() + v.y()*v.y() + v.z()*v.z();
	if (denominator < gcDoubleTolerance)
		return	mgVector_c( 0.0, 0.0, 0.0 );

	return mgVector_c( v.x()/denominator, v.y()/denominator, v.z()/denominator ); 
}

 double vol(mgVector_c const &v1, 
							mgVector_c const &v2, mgVector_c const &v3)
{
	mgVector_c vect = v1*v2;
	return(vect % v3);
}

 BOOL mgVector_c::isOrthogonal ()
{
 	if ( fabs( x() ) < gcLengthTolerance && 
		  fabs( y() ) < gcLengthTolerance &&
		  fabs( z() ) < gcLengthTolerance  )
		return FALSE;   // everything zero

 	if ( fabs( x() ) < gcLengthTolerance && 
		  fabs( y() ) < gcLengthTolerance )
		return TRUE;   // has a Z only

 	if ( fabs( x() ) < gcLengthTolerance && 
		  fabs( z() ) < gcLengthTolerance )
		return TRUE;   // has a Y only

 	if ( fabs( y() ) < gcLengthTolerance && 
		  fabs( z() ) < gcLengthTolerance )
		return TRUE;   // has a X only

	return FALSE;

}


// cast operator to conveniently pass the list to parasolid
mgVector_c::operator double* ()
{ 
	return iComp;
}

 BOOL mgVector_c::normalise()
{
	if (isNull())
		return FALSE;
	*this = mg_Normalise(*this);
	return TRUE;
}
