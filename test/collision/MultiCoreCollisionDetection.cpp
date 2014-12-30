// -*- C++ -*-
// MultiCoreCollisionDetection.cpp : Defines the entry point for the
// console application.

// -- CILK --
// This file has been modified from the original SolidWorks version
// so that it tracks the amount of time it takes to calculate a
// collision between the models.  It also runs the collision multiple
// times to generate statistics.
//
//
// Finally, this program has been Cilkified to make use of multiple-
// core architectures.
//
// Modified by TB Schardl for 6.884 Spring 2010
// Modified by TB Schardl 2014-08

#include "stdafx.h"
#include "aabbtree.h"
#include "vrmlParser.h"

#include <cstdio>

#include <cilk/cilk.h>
// #include <cilktools/cilkview.h>

int main(int argc, char* argv[])
{
  FILE *inFile1, *inFile2, *outFile;
  bool list_collisions = false;

  switch (argc) {
  case 4: // object1, object2, outfile
    list_collisions = true;
    outFile = std::fopen(argv[3], "w");
    if (outFile == NULL) {
      fprintf(stderr, "collision: unable to open output file: %s\n", argv[3]);
      return -1;
    }
    // fall into case 3 to open the other two files.
  case 3: // object1, object2
    inFile1 = std::fopen(argv[1], "r");
    inFile2 = std::fopen(argv[2], "r");
    if (inFile1 == NULL || inFile2 == NULL) {
      fprintf(stderr, "collision: one of the input files could not be opened.\n");
      return -1;
    }
    break;
  default: // need 2 or 3 arguments.
    fprintf(stderr, "Usage: collision OBJECT1 OBJECT2 [OUTPUT]\n");
    return -1;
  }

  vrmlParser *p = new vrmlParser();
  moTessMesh_c* mesh1 = p->parseToMesh(inFile1);
  moTessMesh_c* mesh2 = p->parseToMesh(inFile2);

  if (mesh1 == NULL || mesh2 == NULL) {
    fprintf(stderr, "collision: one of the meshes came out NULL\n");
    return -1;
  }

  /* run the collision detection routine once in order to build the
     octtree.  Tree-building is not included in the execution time. */
  {
    listOut = new aabbTreeCollisionList();
    mesh1->collideWith(mesh2);
    delete listOut;
  }

  /* run the collision detection routine again and time its performance */
  {
    if (list_collisions) { // output a list of collisions to the outfile.
      listOut = new aabbTreeCollisionList();
      // cilk::cilkview cv;

      // cv.start(); {
      mesh1->collideWith(mesh2);
      // } cv.stop();
      //std::printf("collision time: %dms\n", cv.accumulated_milliseconds());
      //cv.dump("collision");

      //outputToBV(outFile, *listOut);
      // cv.start();
      outputToBV(outFile, listOut->get_value());
      // cv.stop();
      // std::printf("collision time: %dms\n", cv.accumulated_milliseconds());
      // cv.dump("collision");

      //delete listOut;
    } else { // just detect _whether_ there are any collisions.  Do not keep a list.
      // cilk::cilkview cv;
      bool collisionFound;

      listOut = NULL;

      // cv.start(); {
      collisionFound = mesh1->collideWith(mesh2);
      // } cv.stop();

      if (collisionFound) {
	std::printf("yes\n");
      } else {
	std::printf("no\n");
      }
      // std::printf("collision time: %dms\n", cv.accumulated_milliseconds());

      // cv.dump("collision");
    }
  }


  return 0;
}
