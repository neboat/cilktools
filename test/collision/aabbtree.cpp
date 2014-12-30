//$c1   XEW 03/31/06 Created 
//========================================================================//
//              Copyright 2006 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	aabbtree.cpp
//     
//     Application:	Axis Aligned Bounding Box methods
//     
//     Contents: Leverages aabb tree to do collision detection on a mesh.
//
//========================================================================//

// #ifdef __cilkplusplus
// #include <cilk.h>
// #endif

#include <cilk/cilk.h>

//BEGIN PCH includes
#include "stdafx.h"
//END PCH includes

#include "aabbtree.h"
//#include <utils/debug/timer.h>
#include "meshprocess.h"
#include "tritri.h"
#include "vector.h"

aabbTreeCollisionList *listOut = 0;
volatile bool doneFlag = false;

// Methods for aabbPoint
aabbPoint::aabbPoint()
{
  iPt[0] = 0.0;
  iPt[1] = 0.0;
  iPt[2] = 0.0;
}

aabbPoint::aabbPoint( double x, double y, double z )
{
  iPt[0] = x;
  iPt[1] = y;
  iPt[2] = z;
}

aabbPoint::aabbPoint( mgVector_c& p0)
{
  p0.get(iPt);
}

aabbPoint::aabbPoint( const aabbPoint& p0 )
{
  memcpy(iPt, p0.iPt,3*sizeof(double));
}

aabbPoint aabbPoint::operator+(aabbPoint const& p) // This will return max
{
  double tmpPt[3];
  for (int i=0; i<3; i++)
    tmpPt[i] = do_max(iPt[i], p.getValue(i));
  
  return aabbPoint(tmpPt[0], tmpPt[1], tmpPt[2]);	
}

aabbPoint aabbPoint::operator-(aabbPoint const& p) // This will return min
{
  double tmpPt[3];
  for (int i=0; i<3; i++)
    tmpPt[i] = do_min(iPt[i], p.getValue(i));
  
  return aabbPoint(tmpPt[0], tmpPt[1], tmpPt[2]);	
}

aabbPoint const& aabbPoint::operator=(const aabbPoint& p)
{
  memcpy(iPt, p.iPt, 3*sizeof(double));
  
  return *this;
}

aabbPoint const& aabbPoint::operator+=(aabbPoint const& p)
{
  for (int i=0; i<3; i++)
    iPt[i] = do_max(iPt[i], p.getValue(i));
  
  return *this;
}

aabbPoint const& aabbPoint::operator-=(aabbPoint const& p) 
{
  for (int i=0; i<3; i++)
    iPt[i] = do_min(iPt[i], p.getValue(i));
  
  return *this;
}

bool aabbPoint::operator==(const aabbPoint& p) const 
{
  double tolerance = aabbTolerance;
  for (int i=0; i<3; i++)
  {
    if (fabs(iPt[i] - p.getValue(i)) > tolerance)
      return false;
  }
  
  return true;
}

void aabbPoint::setPoint( mgVector_c& p0)
{
  for (int i=0; i<3; i++)
    iPt[i] = p0.component(i);
}

// Methods for aabbox

aabbox::aabbox( const aabbox& box )
    : iMin(box.iMin), iMax(box.iMax)
{
}

aabbox aabbox::operator+( aabbox const& p)
{
  aabbPoint tmpMin = iMin - p.iMin;
  aabbPoint tmpMax = iMax + p.iMax;
  
  return aabbox(tmpMin, tmpMax);
}

void aabbox::initialize(mgVector_c& p0)
{
  iMin.setPoint(p0);
  iMax.setPoint(p0);
}

aabbox const& aabbox::operator+=(aabbox const& p)
{
  iMin -= p.iMin;
  iMax += p.iMax;
  
  return *this;
}

aabbox const& aabbox::operator=(aabbox const& p)
{
  iMin = p.iMin;
  iMax = p.iMax;
  
  return *this;
}

aabbox const& aabbox::operator+=(aabbPoint const& p)
{
  iMin -= p;
  iMax += p;
  
  return *this;
}
int aabbox::getMaxDirection(UINT flag) const
{
  int index = -1;
  double maxValue = -1.0;
  for (int i=0; i<3; i++)
  {
    if ( flag != 0 &&
         ((i==0 && (flag & aabb_X_AXIS)) 
          || (i==1 && (flag & aabb_Y_AXIS))
          || (i==2 && (flag & aabb_Z_AXIS)))
         )
      continue;
    
    double tmpValue = iMax.getValue(i) - iMin.getValue(i);
    if (tmpValue > maxValue)
    {
      maxValue = tmpValue;
      index = i;
    }
  }
  
  //	SU_ASSERT(NO_MAX_DIRECTION_FOUND, index != -1);
  return index;
}

UINT aabbox::relativeTo(double value, int index) const
{
  double min = iMin.getValue(index);
  double max = iMax.getValue(index);
  double tolerance = (min - max)*aabbTolerance; // This normalized the tolerance (and makes it negative)
  // So, if value == min or value == max, we will return CONTAINED.
  
  if ( (value - min) < tolerance )
    return aabb_LOWER;
  
  if ( (max - value) < tolerance)
    return aabb_HIGHER;
  
  return aabb_BETWEEN;
}

bool aabbox::collidesWith(const aabbox& boxIn)
{
  if (isEmpty() || boxIn.isEmpty())
    return false;
  
  for (int index =0; index<3; index++)
  {
    double min = iMin.getValue(index);
    double max = iMax.getValue(index);
    double tolerance = (min - max)*aabbTolerance; // Normalized and negative, so approximate collision returns true
    if ( (max - boxIn.iMin.getValue(index)) < tolerance || tolerance > (boxIn.iMax.getValue(index) - min) )
      return false;
  }
  return true;
}

double aabbox::getSize() const
{
  double deltaX = iMax.x() - iMin.x(); // Should be positive, so I will just sum 
  double deltaY = iMax.y() - iMin.y();
  double deltaZ = iMax.z() - iMin.z();
  
  return deltaX+deltaY+deltaZ;
}

// Mothods for aabbTriNode
aabbTriNode::aabbTriNode(aabbTriNode& node)
{
  iBBox = node.iBBox;
  iCenter = node.iCenter;
  iFacet = node.iFacet;
}

void aabbTriNode::initialize( moTessFacet_c* facet)
{
  iFacet = facet;
  if (iFacet != NULL)
  {
    mgVector_c center = iFacet->getVertexCoord(0);
    iBBox.initialize(center);
    for (int i=1; i<iFacet->getSides(); i++)
    {
      mgVector_c vertex = facet->getVertexCoord(i);
      aabbPoint tmpPt( vertex );
      iBBox += tmpPt;
      center += vertex;
    }
    center *= 1.0/(iFacet->getSides());
    iCenter.setPoint(center);
  }
}

aabbTriNode aabbTriNode::operator=(const aabbTriNode& node)
{
  iBBox = node.iBBox;
  iCenter = node.iCenter;
  iFacet = node.iFacet;
  
  return *this;
}

bool aabbTriNode::facetCollideWith(aabbTriNode* nodeIn)
{
  if (nodeIn == NULL || iFacet == NULL || nodeIn->getFacet() == NULL)
    return false;
  
  if (!iBBox.collidesWith(nodeIn->getBBox()))
    return false;
  
  // Now we need to do a complex tri-tri intersection test...
  // Note that this in the only place we assume that we have a triangular facet!
  double tri0[3][3], tri1[3][3];
  for (int i=0; i<3; i++)
  {
    getFacet()->getVertexCoord(i).get(tri0[i]);
    nodeIn->getFacet()->getVertexCoord(i).get(tri1[i]);
  }
  
  bool returnValue = !!moTriTriIntersect(tri0[0], tri0[1], tri0[2], tri1[0], tri1[1], tri1[2], false); 
  if (returnValue && listOut != NULL) {
    //list_mutex.lock();
    (*listOut).push_back(aabbFacetPair(getFacet(), nodeIn->getFacet()));
    // (*listOut)->push_back(aabbFacetPair(getFacet(), nodeIn->getFacet()));
    //list_mutex.unlock();
  }
  
  return returnValue;
}

//this is for debugging purpose
void outputToBV(FILE *outfile, const std::vector<aabbFacetPair> &l)
// void outputToBV(FILE *outfile, const std::list<aabbFacetPair> &l)
{
  int v=0,f=2*l.size(),pieces=l.size();;
  int patchtype=1,facesides=3;
  fprintf(outfile,"%d\n",patchtype);
  fprintf(outfile,"%d %d\n",3*f, f);
  //moTessFacet_handle facet=currentPtr->itemA->
  // for(std::list<aabbFacetPair>::const_iterator currentPtr = l.begin();
  //     currentPtr != l.end(); ++currentPtr)
  int size = l.size();
  for (int i = 0; i < size; ++i)
  {
    mgVector_c coord;
    for(int i=0;i<3;i++)
    {
      coord = l[i].itemA->getVertexCoord(i);
      // coord=currentPtr->itemA->getVertexCoord(i);
      fprintf(outfile,"%f %f %f\n",coord[0],coord[1],coord[2]);
      v=v+1;
    }
    for(int i=0;i<3;i++)
    {
      coord = l[i].itemB->getVertexCoord(i);
      // coord=currentPtr->itemB->getVertexCoord(i);
      fprintf(outfile,"%f %f %f\n",coord[0],coord[1],coord[2]);
      v=v+1;
    }
  }
  for(int i=0;i<f;i++)
  {
    fprintf(outfile,"%d %d %d %d\n",facesides,3*i+0,3*i+1,3*i+2);
  }
  
  //output color information
  /*fprintf( outfile, "faces %d \n pieces %d \n", f, pieces );
    for( int i=0;i<pieces;i++ )
    {
    fprintf(outfile, "%d\n", i);
    fprintf(outfile, "%d\n", i);
    }
    fclose(outfile);*/
}

// Methods for aabbTreeNode 

bool aabbTreeNode::iCalcBBox()
{
  if (iArraySize == 0)
    return false;
  
  iBBox = iTriNodeArrayPtr[0].getBBox();
  for (int index=1; index<(int)iArraySize; index++)
  {
    iBBox += iTriNodeArrayPtr[index].getBBox();
  }
  
  return !iBBox.isEmpty();
}

/*
 * This is an algorithm for determining the median element, taken from "Computer Algorithms in C++" by Horowitz, etal. section 3.6 (pp 164 - 172)
 */

// partitions the working array by working[low]. Array is ordered so that working[i] < working[splitIndex
int aabbTreeNode::iPartition(double working[], int low, int up) const
{
  double splitElement = working[low];
  int i = low, j = up;
  do
  {
    do
      i++;
    while (working[i] < splitElement);
    do
      j--;
    while (working[j] > splitElement);
    if (i < j)
    {
      double temp = working[i];
      working[i] = working[j];
      working[j] = temp;
    }
  } while (i<j);
  
  working[low] = working[j];
  working[j] = splitElement;
  
  return j;
}

double aabbTreeNode::iGetMedian(int direction) const
{
  if (iArraySize == 0)
    return -1;
  
  if (iArraySize == 1)
    return iTriNodeArrayPtr[0].getCenterPoint().getValue(direction);
  
  double* workingArray = new double[iArraySize+1];
  for (int i=0; i< (int) iArraySize; i++)
    workingArray[i] = iTriNodeArrayPtr[i].getCenterPoint().getValue(direction);
  workingArray[iArraySize] = iBBox.getUpperBound(direction) + 10; // Acts as stopping condition.
  
  int medianIndex = (int) (iArraySize/2);
  int low = 0, up = iArraySize;
  do {
    int j = iPartition(workingArray, low, up); // partitions the list by element workingArray[low]
    if (medianIndex == j)
      return workingArray[medianIndex];
    else if (medianIndex  < j)
      up = j;
    else
      low = j+1;
  } while (true);
  
  // return -1;
}

/*
 * This method uses recursion to construct an Axis Aligned Bounding
 * Box Tree.  It is essentially a trinary version of partitioning
 * algorithm in getMedian...
 */
aabbTreeNode::~aabbTreeNode()
{
  if (iLowerChild)
    delete iLowerChild;
  
  if (iMiddleChild)
    delete iMiddleChild;
  
  if (iUpperChild)
    delete iUpperChild;
}


void aabbTreeNode::construct()
{
  if (iArraySize == 0) // if the array is empty  stop
    return;
  
  if (iBBox.isEmpty() && !iCalcBBox()) // Make sure we calc bbox for single elements...
    return;
  
  if (iArraySize == 1) // if the array is has one element stop
    return;
  
  if ( iDirectionsDone == (aabb_X_AXIS | aabb_Y_AXIS | aabb_Z_AXIS) ) // if all directions exhausted, stop
    return;
  
  int direction = iBBox.getMaxDirection(iDirectionsDone);
  
  double median = iGetMedian(direction);
  
  // Mark the direction we are doing. This is acts as a terminal step
  // for the middle elements.
  if (direction == 0)
    iDirectionsDone |= aabb_X_AXIS;
  else if (direction == 1)
    iDirectionsDone |= aabb_Y_AXIS;
  else if (direction == 2)
    iDirectionsDone |= aabb_Z_AXIS;
  
  /*
   * This is a three way partitioning.  Since the test is relatively
   * cheap, while the swap is a little costly, I am going to make one
   * pass through to get the size of the partitions.
   */
  int lowerCnt=0, midCnt=0, highCnt=0, i=0;
  for (i=0; i<(int)iArraySize; i++)
  {
    switch (iTriNodeArrayPtr[i].relativeTo( median, direction ))
    {
      case aabb_LOWER:
        lowerCnt++;
        break;
        
      case aabb_BETWEEN:
        midCnt++;
        break;
        
      case aabb_HIGHER:
        highCnt++;
        break;
        
      default:
        break;
    }
  };
  
  if ( lowerCnt == iArraySize || midCnt == iArraySize || highCnt == iArraySize )
  {
    // Note - the direction will act as a terminus for this recursion.
    construct();
    return;
  }
  
  /*
   * Now, I only need to move through the lower and middle partition.
   * I'll push items to the back of respective partition.  Making sure
   * not to swap with properly positioned items.
  */
  int index = 0;
  int midSwap = lowerCnt + midCnt;
  int highSwap = iArraySize;
  UINT flag = iTriNodeArrayPtr[0].relativeTo( median, direction );
  
  while (index < lowerCnt)
  {
    while (flag == aabb_LOWER) // Note: there must at least be one in middle or upper
    {
      flag = iTriNodeArrayPtr[++index].relativeTo( median, direction );
    }
    
    if (index < lowerCnt)
    {
      if (flag == aabb_BETWEEN)
      {
        while (flag == aabb_BETWEEN) // Note: we have to hit this at least once...
        {
          flag = iTriNodeArrayPtr[--midSwap].relativeTo( median, direction );
        }
        aabbTriNode tmpNode = iTriNodeArrayPtr[midSwap];
        iTriNodeArrayPtr[midSwap] = iTriNodeArrayPtr[index];
        iTriNodeArrayPtr[index] = tmpNode;		
      }
      else if (flag == aabb_HIGHER)
      {
        while (flag == aabb_HIGHER) // Note: we have to hit this at least once...
        {
          flag = iTriNodeArrayPtr[--highSwap].relativeTo( median, direction );
        }
        aabbTriNode tmpNode = iTriNodeArrayPtr[highSwap];
        iTriNodeArrayPtr[highSwap] = iTriNodeArrayPtr[index];
        iTriNodeArrayPtr[index] = tmpNode;
      }
    }
  }
  
  // At this point, the lower partition is completely filled in. We only need to do the middle and then only if the upper is not empty.
  if (highCnt != 0)
  {
    while (index < midSwap)
    {
      while (flag == aabb_BETWEEN)
      {
        flag = iTriNodeArrayPtr[++index].relativeTo( median, direction );
      }
      
      if (index < midSwap && flag == aabb_HIGHER)
      {
        while (flag == aabb_HIGHER) // Note: we have to hit this at least once...
        {
          flag = iTriNodeArrayPtr[--highSwap].relativeTo( median, direction );
        }
        aabbTriNode tmpNode = iTriNodeArrayPtr[highSwap];
        iTriNodeArrayPtr[highSwap] = iTriNodeArrayPtr[index];
        iTriNodeArrayPtr[index] = tmpNode;
      }
    }
  }
  
  // Now construct the children
  // This is recursive
  if (lowerCnt != 0)
  {
    iLowerChild = new aabbTreeNode( iTriNodeArrayPtr, lowerCnt );
  }
  
  if (midCnt != 0)
  {
    iMiddleChild = new aabbTreeNode( &(iTriNodeArrayPtr[lowerCnt]), midCnt, iDirectionsDone );
  }
  
  if (highCnt != 0)
  {
    iUpperChild = new aabbTreeNode( &(iTriNodeArrayPtr[lowerCnt+midCnt]), highCnt );
  }
}

// Idea
bool aabbTreeNode::collideWith(aabbTreeNode* nodeIn) {
  if (nodeIn == NULL || getNumFacets() == 0 || nodeIn->getNumFacets() == 0)
    return false;

  double size = getSize();
  double difference = size - nodeIn->getSize();
  double localTol = -size*aabbTolerance;
  // Alway check larger against smaller (this causes a complex, but efficient traversal of the trees).
  if (difference < localTol)
    return nodeIn->collideWith(this);

  // If they do not collide, return false;
  if (!iBBox.collidesWith(nodeIn->getBBox()))
    return false;

  // If this is a leaf, check at the facet level
  if (isLeaf())
    return nodeIn->facetCollideWith(this);

  //else test against each child. Note: null checks and size checks occur above, so they do not need repeating here...
  if (listOut == NULL) {
    if (nodeIn->collideWith(iLowerChild))
      return true;
    
    if (nodeIn->collideWith(iMiddleChild))
      return true;

    if (nodeIn->collideWith(iUpperChild))
      return true;

    return false;
//     bool iLowerFlag, iMiddleFlag, iUpperFlag;
//     iLowerFlag  = cilk_spawn nodeIn->collideWith(iLowerChild);
//     iMiddleFlag = cilk_spawn nodeIn->collideWith(iMiddleChild);
//     iUpperFlag  =            nodeIn->collideWith(iUpperChild);

//     return iLowerFlag || iMiddleFlag || iUpperFlag;
  } else {
    cilk_spawn nodeIn->collideWith(iLowerChild);
    cilk_spawn nodeIn->collideWith(iMiddleChild);
    nodeIn->collideWith(iUpperChild);
    cilk_sync;
  }

  return false;
}

bool aabbTreeNode::facetCollideWith(aabbTreeNode* nodeIn)
{
  if (nodeIn == NULL) 
    return false;
  
  // If they do not collide, return false;
  if (!iBBox.collidesWith(nodeIn->getBBox()))
    return false;
  
  // If this is a leaf, check at the facet level
  bool returnValue = false;
  if (isLeaf())
  {
    for (int i = 0; i<getNumFacets(); i++)
      for (int j=0; j<nodeIn->getNumFacets(); j++)
      {
        returnValue |= getTriNode(i)->facetCollideWith(nodeIn->getTriNode(j));
        if ( returnValue && listOut == NULL)
          return true;
      }
  }
  else
  {
    //else test against each child. 
    returnValue |= nodeIn->facetCollideWith(iLowerChild);
    if ( returnValue && listOut == NULL)
      return true;
    
    returnValue |= nodeIn->facetCollideWith(iMiddleChild);
    if ( returnValue && listOut == NULL)
      return true;
    
    returnValue |= nodeIn->facetCollideWith(iUpperChild);
    if ( returnValue && listOut == NULL)
      return true;
  }
  
  return returnValue;
}

aabbTopTreeNode::aabbTopTreeNode( moTessMesh_c* mesh ): aabbTreeNode()
{
  iArraySize = mesh->getFacetNum();
  if (iArraySize == 0)
  {
    iTriNodeArrayPtr = NULL;
  }
  else
  {
    iTriNodeArrayPtr = new aabbTriNode[iArraySize];
    for (UINT i=0; i<iArraySize; i++)
      iTriNodeArrayPtr[i].initialize(mesh->getFacetHandle(i));
    
    construct();
  }
}

aabbTopTreeNode::~aabbTopTreeNode()
{
  if (iTriNodeArrayPtr)
    delete [] iTriNodeArrayPtr;
}
