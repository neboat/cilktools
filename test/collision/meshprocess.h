// -*- C++ -*-
//$c1   XEW 03/31/06 Created 
//========================================================================//
//              Copyright 2006 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	meshprocess.h
//     
//     Application:	Mesh processor
//     
//     Contents: This provides a general frame work for writing mesh processing routines.
//
//========================================================================//
#ifndef __MO_MESHPROCESS_INCLUDED__
#define __MO_MESHPROCESS_INCLUDED__

#include <cilk/cilk.h>

//============================================================================//
class moTessVertex_c;
class moTessFacet_c;
class moTessMesh_c;
class aabbTopTreeNode;

typedef moTessVertex_c  * moTessVertex_handle;
typedef moTessFacet_c   * moTessFacet_handle;


class moTessVertex_c {

 public:
  moTessVertex_c(): iIndex(0) {};
  moTessVertex_c(const mgVector_c& coordIn, int index): vtxCoord(coordIn), iIndex(index) {};
  moTessVertex_c(float* coordIn, int index): iIndex(index) {
    vtxCoord[0] = coordIn[0];
    vtxCoord[1] = coordIn[1];
    vtxCoord[2] = coordIn[2];
  };

  int getIndex() {return iIndex;};
  
  const mgVector_c & getCoord() {return vtxCoord;};
  void setCoord(const mgVector_c& vectorIn);
  
  BOOL coordEqual(moTessVertex_handle v);
  
 private:
  mgVector_c vtxCoord;
  int iIndex;
};


class moTessFacet_c {

 public:
  moTessFacet_c() {}
  moTessFacet_c(moTessVertex_handle v1, moTessVertex_handle v2, moTessVertex_handle v3);
  ~moTessFacet_c();
  
  // accessors
  int                   getSides()     {return 3;};
  moTessVertex_handle   getVertexHandle(int index) {return iVertexHandles[index];};
  const mgVector_c&	  getVertexCoord   (int index) {return getVertexHandle(index)->getCoord();};
  
  void				  setVertexHandle(int index, moTessVertex_handle vertex);
  
  // functions for convex decomposition
  void				  computePlane();
  const mgVector_c&     getPlaneNormal()   {return iPlaneNormal;};
  double                 getPlaneDistance() {return iPlaneDistance;};
  

  
 private:
  moTessVertex_handle iVertexHandles[3];
  
  // attributes for convex decomposition
  mgVector_c  iPlaneNormal;
  double		iPlaneDistance;
  
};


class moTessMesh_c {

 public:
  moTessMesh_c(int numVertices, int numFacets);
  moTessMesh_c():iAABBTree(NULL), iVertexSize(0), iMaxVertexSize(0), iFacetSize(0), iMaxFacetSize(0) {};
  ~moTessMesh_c();
  
  // accessors
  int getVertexNum()    const {return iVertexSize;}
  int getFacetNum()     const {return iFacetSize;}
  
  moTessFacet_handle    getFacetHandle   (int index) const {return iFacets[index];};
  moTessVertex_handle   getVertexHandle  (int index) const {return &iVertices[index];};
  const mgVector_c &	  getVertexCoord   (int index) const {return getVertexHandle(index)->getCoord();};
  
  // construction
  void				  initizalize(int numVertices, int numFacets);
  void				  addVertex(float* coordIn);
  void				  addVertex(const mgVector_c & coordIn);
  moTessFacet_handle    addFacet(int v1, int v2, int v3);
  
  void addTriangle(float* v1, float* v2, float* v3 );
  
  void clear();
  void clearVertices();
  void clearFacets();
  void setVertices(moTessVertex_c* iNewVertices);
  
  // mess processos
  moTessFacet_handle getFacetFromVertices(moTessVertex_handle v1, moTessVertex_handle v2, moTessVertex_handle v3) const;
  
  bool collideWith(moTessMesh_c* meshIn);
  
  aabbTopTreeNode* getAABBTree();
  void initializeAABBTree();
  
 protected:
  void computeFacetPlanes();
  
 protected:
  moTessVertex_c*   iVertices;
  moTessFacet_c**    iFacets;
  
  int iVertexSize;
  int iMaxVertexSize;
  int iFacetSize;
  int iMaxFacetSize;
  
  aabbTopTreeNode* iAABBTree;
};

extern BOOL phx_TimingIsOn;

#ifdef _DEBUG

#define PHX_MAKE_TIMER( _theTimer, where ) suTimer_c _theTimer( where );
 
#define PHX_START_TIMING( _theTimer ) if( phx_TimingIsOn ) _theTimer.start(); 

#define PHX_REPORT_TIME( _theTimer, what ) if( phx_TimingIsOn ) _theTimer.report( what );

#define PHX_END_TIMING( _theTimer ) if( phx_TimingIsOn ) _theTimer.end();

#define PHX_PAUSE_TIMING( _theTimer ) if( phx_TimingIsOn ) _theTimer.pause();

#define PHX_CONTINUE_TIMING( _theTimer ) if( phx_TimingIsOn ) _theTimer.continueTiming(); 

#else

#define PHX_MAKE_TIMER( _theTimer, where ) 
 
#define PHX_START_TIMING( _theTimer ) 

#define PHX_REPORT_TIME( _theTimer, what ) 

#define PHX_END_TIMING( _theTimer ) 

#define PHX_PAUSE_TIMING( _theTimer ) 

#define PHX_CONTINUE_TIMING( _theTimer ) 

#endif

#endif //__MO_MESHPROCESS_INCLUDED__
