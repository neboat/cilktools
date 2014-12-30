//$c1   JHF 06/19/07 Created for parsing vrml format file
//========================================================================//
//              Copyright 2007 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	vrmlParser.h
//     
//     Application:	parsing vrml
//     
//     Contents: 
//
//========================================================================//
extern "C++" {
#include <stdio.h>
}
#include "aabbtree.h"
#include "meshprocess.h"
class vrmlParser
{
public:
	vrmlParser();
	~vrmlParser(){delete[] faceArr;delete[] vertexArr;}
	void parseToArr(FILE* rFile);
	void parse(const char* vrmlFile, char* meshFile);
	void parseToFile(FILE* rFile,FILE* wFile);
	moTessMesh_c* parseToMesh(FILE* rFile);
	void init();
private:
	int fsize;
	int vsize;
	int numOfPoints;
	int numOfFaces;
	int *faceArr;
	float *vertexArr;
};
