// -*- C++ -*-
//$c1   KDH 03/15/07 Created 
//========================================================================//
//              Copyright 2007 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	aabbtree.h
//     
//     Application:	Axis Aligned Bounding Box methods
//     
//     Contents: This is created for collision project. It leverages
//				aabb tree to do collision detection on a mesh.
//
//========================================================================//
#ifndef __MO_AABBTREE_INCLUDED__
#define __MO_AABBTREE_INCLUDED__

#include <cilk/cilk.h>
#include <list>
#include <vector>
// #include <cilk/cilk_mutex.h>
// #include <cilk/reducer_list.h>
#include "reducer_vector.h"

class moTessFacet_c;
class mgVector_c;
class moTessMesh_c;

enum aabbFlags_e {aabb_NONE = 0x0, aabb_X_AXIS = 0x01, aabb_Y_AXIS = 0x02, aabb_Z_AXIS = 0x04};
enum aabbReturnFlags_e {aabb_EMPTY = 0x0, aabb_LOWER = 0x01, aabb_BETWEEN = 0x02, aabb_HIGHER = 0x04};
#define aabbTolerance 0.000000001;

class aabbPoint
{
 public:
  aabbPoint();
  aabbPoint( double x, double y, double z );
  aabbPoint( mgVector_c& p0);
  aabbPoint( const aabbPoint& p0 );
  
  double getValue(int index) const {return iPt[index];} // note: for performance reasons, I do not check bounds!!
  double x() const {return getValue(0);}
  double y() const {return getValue(1);}
  double z() const {return getValue(2);}
  
  aabbPoint operator+(aabbPoint const& p); // This will return max
  aabbPoint operator-(aabbPoint const& p); // This will return min
  aabbPoint const& operator=(const aabbPoint& p); 
  aabbPoint const& operator+=(aabbPoint const& p); 
  aabbPoint const& operator-=(aabbPoint const& p); 
  bool operator==(const aabbPoint& p) const; 
  
  void setPoint( mgVector_c& p0);
  
 private:
  double iPt[3];
};

class aabbox
{
 public:
  aabbox(): iMin(0.0,0.0,0.0) {;}
  aabbox(mgVector_c& p0): iMin(p0), iMax(p0) {;}
  aabbox(aabbPoint& pMin, aabbPoint& pMax): iMin(pMin), iMax(pMax) {;}
  aabbox(const  aabbox& box );
  
  void initialize(mgVector_c& p0);
  
  bool isEmpty() const {return (iMin == iMax);}
  
  aabbox operator+( aabbox const& p); // This will union bboxes
  aabbox const& operator+=(aabbox const& p); 
  aabbox const& operator=(aabbox const& p); 
  aabbox const& operator+=(aabbPoint const& p); 
  
  int getMaxDirection(UINT flag = aabb_NONE) const;
  UINT relativeTo(double value, int index) const;
  double getUpperBound(int direction) const {return iMax.getValue(direction);}
  
  bool collidesWith(const aabbox& boxIn);
  double getSize() const;
  
 private:
  aabbPoint iMin, iMax;
};

extern "C++"
struct aabbFacetPair
{
  moTessFacet_c* itemA;
  moTessFacet_c* itemB;
  aabbFacetPair(moTessFacet_c *a, moTessFacet_c *b) 
      : itemA(a), itemB(b) { }
  aabbFacetPair()
      : itemA(NULL), itemB(NULL) { }
  
};

typedef void outputToBV_type(FILE *);

void outputToBV(FILE *, const std::vector<aabbFacetPair> &);
// void outputToBV(FILE *, const std::list<aabbFacetPair> &);

// typedef std::list<aabbFacetPair> aabbTreeCollisionList;
// typedef cilk::reducer< cilk::op_list_append<aabbFacetPair> > aabbTreeCollisionList;
typedef reducer_basic_vector<aabbFacetPair> aabbTreeCollisionList;

extern aabbTreeCollisionList *listOut;
//cilk::mutex list_mutex;
extern volatile bool doneFlag;

class aabbTriNode
{
 public:
  aabbTriNode() : iFacet(NULL) {;}
  aabbTriNode(aabbTriNode& node);
  
  void initialize( moTessFacet_c* facet);
  
  aabbTriNode operator=(const aabbTriNode& node);
  
  const aabbox& getBBox() const {return iBBox;}
  const aabbPoint& getCenterPoint() const {return iCenter;}
  moTessFacet_c* getFacet() const {return iFacet;}
  
  UINT relativeTo(double value, int index) const {return iBBox.relativeTo(value, index);}
  bool facetCollideWith(aabbTriNode* nodeIn);
  
 private:
  aabbox iBBox;
  aabbPoint iCenter;
  moTessFacet_c* iFacet;
};


class aabbTreeNode
{
 public:
  aabbTreeNode() : iTriNodeArrayPtr(NULL), iArraySize(0), iDirectionsDone(aabb_NONE), iLowerChild(NULL), iMiddleChild(NULL), iUpperChild(NULL) {;}
  aabbTreeNode(aabbTriNode* triNodes, UINT size, UINT flags = aabb_NONE) : iTriNodeArrayPtr(triNodes), iArraySize(size), iDirectionsDone(flags),
                                                                           iLowerChild(NULL), iMiddleChild(NULL), iUpperChild(NULL) {construct();}
  ~aabbTreeNode();
  
  void construct();
  bool collideWith(aabbTreeNode* nodeIn);
  bool facetCollideWith(aabbTreeNode* nodeIn);
  const aabbox& getBBox() const {return iBBox;}
  double getSize() const {return getBBox().getSize();}
  
  int getNumFacets() const {return iArraySize;}
  aabbTriNode* getTriNode(int index) {return &(iTriNodeArrayPtr[index]);}
  
 protected:
  void iSetTriNodeArray(aabbTriNode* triNodes, UINT size) {iTriNodeArrayPtr = triNodes; iArraySize = size;}
  int iPartition(double working[], int start, int end) const;
  double iGetMedian(int direction) const;
  bool iCalcBBox();
  bool isLeaf() {return (iLowerChild == NULL && iMiddleChild == NULL && iUpperChild == NULL);}
  
 protected:
  aabbTriNode* iTriNodeArrayPtr;
  UINT iArraySize;
  UINT iDirectionsDone;
  aabbox iBBox;
  
  aabbTreeNode* iLowerChild;
  aabbTreeNode* iMiddleChild;
  aabbTreeNode* iUpperChild;
};

class aabbTopTreeNode : public aabbTreeNode
{
 public:
  aabbTopTreeNode( moTessMesh_c* mesh );
  
  ~aabbTopTreeNode();
  
};

#endif
