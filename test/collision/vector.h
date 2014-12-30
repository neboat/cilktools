// -*- C++ -*-
//========================================================================//
//              Copyright 2006 (Unpublished Material)                     //
//                  SolidWorks Corporation.                           //
//========================================================================//
//
//     File Name: vector.h
//     
//        
//     
//     Contents: class for simple 3d vector
//
//========================================================================//
#ifndef VECTOR_INCLUDED
#define VECTOR_INCLUDED

#include <cstring>

class mgUnitVector_c;
class mgVector_c;

#ifdef _DEBUG
#define TRACE_VECTOR(vec) TRACE(_T("%s %.8f %.8f %.8f\n"), _T(#vec), vec.x(), vec.y(), vec.z());
#endif

class mgVector_c
{
public:
//====
// constructors
//====
  // Construct a mgVector_c from three doubles.
  mgVector_c( double x, double y, double z );
  mgVector_c( const double v[ 3 ] );
  mgVector_c(  );							// 1,0,0
  mgVector_c( const mgVector_c& v );   		// copy
  
  // assignment
  mgVector_c const&	 operator=( const mgVector_c& v ); 	// assignment

  // Destructor
#ifdef _WIN32
  __thiscall
#endif
  ~mgVector_c();

  // Extract the components of a mgVector_c.
  double x() const;
  double y() const;
  double z() const;
  void get(double coor[3]) const;
  void get(float coor[3]) const;
  
  double component( int i ) const;
  
  // Set component values.
  void set( double v[3] );
  void set( double new_x, double new_y, double new_z = 0.0);
  void set_x( double new_x );
  void set_y( double new_y );
  void set_z( double new_z );
  
  // Unary minus.
  friend  mgVector_c operator-( mgVector_c const & );
  
  // Binary minus.
  friend  mgVector_c operator-( mgVector_c const &, mgVector_c const & );
  
  // Addition of mgVector_cs.
  friend  mgVector_c operator+( mgVector_c const &, mgVector_c const & );
  mgVector_c const &operator+=( mgVector_c const & );

  // Multiplication of a mgVector_c by a scalar.
  friend  mgVector_c operator*( double, mgVector_c const & );
  friend  mgVector_c operator*( mgVector_c const &, double );
  mgVector_c const &operator*=( double );

  // Scalar product of two mgVector_cs.
  friend  double operator%( mgVector_c const &, mgVector_c const & );

  // Cross product of general mgVector_cs. Also applies to unit mgVector_cs.
  friend  mgVector_c operator*( mgVector_c const &, mgVector_c const & );

  // Division of a mgVector_c by a scalar.
  friend  mgVector_c operator/( mgVector_c const &, double );

  // Form a mgUnitVector_c by normalising a mgVector_c.
  friend  mgVector_c mg_Normalise( mgVector_c const & );

  // Length of a mgVector_c.
  double sqlen() const { return 
        iComp[0]*iComp[0]+iComp[1]*iComp[1]+iComp[2]*iComp[2]; }
  
  double len() const { return sqrt (sqlen()) ; }
  
  friend  double vol(mgVector_c const &v1, 
                     mgVector_c const &v2, mgVector_c const &v3);
  
  BOOL isOrthogonal ();
  
  //	Check for not null
  BOOL isNotNull() const;
  
  //	Check for null
  BOOL isNull() const;
  
  //	Check the null (internal data) flag
  BOOL null() const;
  
  // normalize in place.
  BOOL normalise();
  
  friend  BOOL operator||( mgVector_c const&, mgVector_c const& );
  
  //	Check equality within tolerance
  friend  BOOL operator==( mgVector_c const&, mgVector_c const& );	
  friend  BOOL operator!=( mgVector_c const&, mgVector_c const& );	
  
  // cast operator to conveniently pass the list to parasolid
  operator double* () ;
 private:
  
  //	Private Data
  double	iComp[3];
	
};

//=======================================================================
//	Definition of INLINE (data access) member functions
//=======================================================================
inline	double mgVector_c::x() const 
{ 
  return ( iComp[0] ); 
}

inline	double mgVector_c::y() const 
{ 
  return ( iComp[1] ); 
}

inline	double mgVector_c::z() const 
{ 
  return ( iComp[2] ); 
}

inline	double mgVector_c::component( int i ) const 
{ 
  return ( iComp[i] ); 
}

inline	BOOL	mgVector_c::null() const
{
  return (iComp[0] == 0.0 && iComp[1] == 0.0 && iComp[2] == 0.0);
}

inline void mgVector_c::get(double coor[3]) const
{
  memcpy( coor, iComp, 3 * sizeof( double ) );   
}

inline void mgVector_c::get(float coor[3]) const
{
  coor[0] = (float)iComp[0];
  coor[1] = (float)iComp[1];
  coor[2] = (float)iComp[2];
}

#endif
