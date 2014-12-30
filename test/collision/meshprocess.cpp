// -*- C++ -*-
//========================================================================//
//              Copyright 2006 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	meshprocess.cpp
//     
//     Application:	Mesh processor
//     
//     Contents: This provides
//                a general frame work for writing other mesh processing routines.
//
//========================================================================//

//BEGIN PCH includes
#include "stdafx.h"
//END PCH includes

#include "aabbtree.h"
#include "meshprocess.h"
#include "tritri.h"

//#include <utils/debug/timer.h>

// extern "C++" {
#include <stack>
// }

BOOL phx_TimingIsOn = FALSE;


////////////////////////////////////////////////////
// vertex

BOOL moTessVertex_c::coordEqual(moTessVertex_handle v)
{
  return (getCoord() == v->getCoord());
}

void moTessVertex_c::setCoord(const mgVector_c& vectorIn)
{
  vtxCoord.set(vectorIn.x(), vectorIn.y(), vectorIn.z());
}


////////////////////////////////////////////////////
// facet

moTessFacet_c::moTessFacet_c(moTessVertex_handle v1, moTessVertex_handle v2, moTessVertex_handle v3)
{
  iVertexHandles[0] = v1;
  iVertexHandles[1] = v2;
  iVertexHandles[2] = v3;
}

moTessFacet_c::~moTessFacet_c() {
}


void moTessFacet_c::setVertexHandle(int index, moTessVertex_handle vertex)
{
	iVertexHandles[index] = vertex;
}

void moTessFacet_c::computePlane()
{
  iPlaneNormal = (getVertexCoord(2)-getVertexCoord(1))*(getVertexCoord(0)-getVertexCoord(1));
  
  iPlaneNormal.normalise();
  
  double distance1 = iPlaneNormal % getVertexCoord(0);
  double distance2 = iPlaneNormal % getVertexCoord(1);
  double distance3 = iPlaneNormal % getVertexCoord(2);
  
  iPlaneDistance = (distance1 + distance2 + distance3)/3.0;
}

//////////////////////////////////////////////////////////////////////
// mesh


moTessMesh_c::~moTessMesh_c()
{
  clear();
}

void moTessMesh_c::clear()
{
  clearVertices();
  clearFacets();
  SU_DELETE iAABBTree;
  iAABBTree = NULL;
}



void moTessMesh_c::clearVertices()
{
  SU_DELETE [] iVertices;
  iMaxVertexSize = 0;
  iVertexSize = 0;
}

void moTessMesh_c::clearFacets()
{
  int fNum = getFacetNum();
  for(int i=0; i<fNum; i++) {
    SU_DELETE(iFacets[i]);
  }
  SU_DELETE [] iFacets;
}

// ----------------------
// construction routines
// ----------------------
moTessMesh_c::moTessMesh_c(int numVertices, int numFacets) : iVertexSize(0), iMaxVertexSize(numVertices), iFacetSize(0), iMaxFacetSize(numFacets), iAABBTree(NULL)
{
  iVertices = SU_NEW moTessVertex_c[iMaxVertexSize];
  iFacets = SU_NEW moTessFacet_handle[iMaxFacetSize];
  for (int i=0; i<iMaxFacetSize; i++)
    iFacets[i] = NULL;
}

void moTessMesh_c::addVertex(float* coordIn)
{
  mgVector_c temp;
  temp.set(coordIn[0], coordIn[1], coordIn[2]);
  iVertices[iVertexSize++].setCoord(temp);
}

void moTessMesh_c:: addVertex(const mgVector_c & coordIn)
{
  iVertices[iVertexSize++].setCoord(coordIn);
}

void moTessMesh_c::addTriangle(float* coord1, float* coord2, float* coord3 )
{
  int vIndex = getVertexNum();
  
  addVertex(coord1);
  addVertex(coord2);
  addVertex(coord3);
  
  addFacet(vIndex, vIndex+1, vIndex+2);
}

moTessFacet_handle moTessMesh_c::addFacet(int v1, int v2, int v3)
{
  moTessFacet_handle facet = SU_NEW moTessFacet_c(getVertexHandle(v1), getVertexHandle(v2), getVertexHandle(v3));
  iFacets[iFacetSize++] = facet;
  return facet;
}



////////////////////////////////////////////////////////////////////////
// processing routines
////////////////////////////////////////////////////////////////////////

void moTessMesh_c::computeFacetPlanes ()
{
  int f;
  for(f=0;f<getFacetNum();f++)
  {
    moTessFacet_handle facet = getFacetHandle(f);
    facet->computePlane();
  }
}

void moTessMesh_c::initializeAABBTree()
{
  if (iAABBTree != NULL)
    return;
  
  iAABBTree = SU_NEW aabbTopTreeNode(this);
}

bool  moTessMesh_c::collideWith(moTessMesh_c* meshIn)
{
  if (getAABBTree() == NULL || meshIn == NULL)
    return FALSE;
  
  bool returnValue = getAABBTree()->collideWith(meshIn->getAABBTree());
  
  //return returnValue || (listOut != NULL && !(listOut->get_value().empty()));
  return returnValue;
}

aabbTopTreeNode* moTessMesh_c::getAABBTree() 
{
  if (iAABBTree == NULL)
    initializeAABBTree();
  
  return iAABBTree;
}
