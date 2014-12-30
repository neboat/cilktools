// -*- C++ -*-
//$c1   JHF 06/19/07 Created for parsing vrml format file
//========================================================================//
//              Copyright 2007 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	vrmlParser.cpp
//     
//     Application:  parsing vrml
//     
//     Contents: 
//               
//               
//
//========================================================================//
//BEGIN PCH includes

#ifdef _WIN32
// Windows

#else
// Linux
#define fscanf_s fscanf
#define fopen_s(a, b, c) (*(a)) = fopen(b, c)
#endif

#include "stdafx.h"
//END PCH includes

#include "vrmlParser.h"
//#include <iostream>
// extern "C++" {
#include <string>
// }
using namespace std;

vrmlParser::vrmlParser():fsize(300),vsize(300),numOfPoints(0),numOfFaces(0)
{
	faceArr = new int[fsize];
	vertexArr = new float[vsize];
}
void vrmlParser::parseToArr(FILE *rFile)
{
  char c;	char s[41];
  float x,y,z;
  int i,j,k,m;
  int offset=0;
  string coordstr = string("oordinate3");
  string indexstr = string("oordIndex");
  string pointstr = string("point");
  //faceArr = new int[size];coordIn= new float[3];
  //vertexArr = new float[vsize];
  if (rFile==NULL) {printf("empty file\n"); return ;}
  // fscanf_s(rFile,"%c",&c,1); //printf("%c",c);
  fscanf_s(rFile,"%c",&c); //printf("%c",c);

  while(!feof(rFile)) //while(c!=EOF)
  {
    if(c=='C')
    {
      // fscanf_s(rFile,"%s",s,40);//printf("%s",s);
      fscanf_s(rFile,"%s",s);//printf("%s",s);
      if(coordstr.compare(s)==0)//point coordinates
      {
        // fscanf_s(rFile,"%s",s,40); if(string("{").compare(s)!=0){printf("format error\n");return ;}
        // fscanf_s(rFile,"%s",s,40);  if(pointstr.compare(s)!=0){printf("format error\n");return ;}
        // fscanf_s(rFile,"%s",s,40);//offset = numOfPoints;
        fscanf_s(rFile,"%s",s); if(string("{").compare(s)!=0){printf("format error\n");return ;}
        fscanf_s(rFile,"%s",s);  if(pointstr.compare(s)!=0){printf("format error\n");return ;}
        fscanf_s(rFile,"%s",s);//offset = numOfPoints;
        while(string("]").compare(s)!=0)
        {
          fscanf_s(rFile,"%f %f %f",&x,&y,&z);
          if(3*numOfPoints+3>=vsize)
          {
            float *temp = new float[2*vsize];
            for(int ind=0;ind<vsize;ind++)
              temp[ind]=vertexArr[ind];
            delete [] vertexArr;
            vertexArr = temp;vsize=2*vsize;
            
          }
          vertexArr[3*numOfPoints]=x;
          vertexArr[3*numOfPoints+1]=y;
          vertexArr[3*numOfPoints+2]=z;
          numOfPoints++;
          // fscanf_s(rFile,"%s",s,40);//printf("%s",s);
          fscanf_s(rFile,"%s",s);//printf("%s",s);
        }
      }
    }else if(c=='c')
    {   
      // fscanf_s(rFile,"%s",s,40);
      fscanf_s(rFile,"%s",s);
      if(indexstr.compare(s)==0)//index
      {
        // fscanf_s(rFile,"%s",s,40);//printf("%s",s);
        fscanf_s(rFile,"%s",s);//printf("%s",s);
        while(string("]").compare(s)!=0)
        {
          fscanf_s(rFile,"%d, %d, %d, %d",&i,&j,&k,&m);
          //printf("%d %d %d %d",i,j,k,m);
          if(3*numOfFaces+3>=fsize)
          {
            int *temp = new int[2*fsize];
            for(int ind=0;ind<fsize;ind++)
              temp[ind]=faceArr[ind];
            delete [] faceArr;
            faceArr = temp;fsize=2*fsize;
            
          }
          faceArr[3*numOfFaces]=i+offset;
          faceArr[3*numOfFaces+1]=j+offset;
          faceArr[3*numOfFaces+2]=k+offset;
          //printf("%d %d %d\n",faceArr[3*numOfFaces],faceArr[3*numOfFaces+1],faceArr[3*numOfFaces+2]);
          numOfFaces++;
          // fscanf_s(rFile,"%s",s,40);
          fscanf_s(rFile,"%s",s);
        }
        offset = numOfPoints;
        //printf("#####################################\n");
      }
    }
    // fscanf_s(rFile,"%c",&c,1);//printf("%c",c);
    fscanf_s(rFile,"%c",&c);//printf("%c",c);
  }
  fclose(rFile);
}
void vrmlParser::parseToFile(FILE* rFile,FILE* wFile)
{
  parseToArr(rFile);
  fprintf(wFile,"%d %d\n",numOfPoints,numOfFaces);
  for(int id=0;id<numOfPoints;id++)
  {
    fprintf(wFile,"%f %f %f\n",vertexArr[3*id],vertexArr[3*id+1],vertexArr[3*id+2]);
  }
  for(int id=0;id<numOfFaces;id++)
  {
    fprintf(wFile,"%d %d %d\n",faceArr[3*id],faceArr[3*id+1],faceArr[3*id+2]);
  }
  delete [] vertexArr;
  delete [] faceArr;
  init();
  fclose(wFile);
}
void vrmlParser::parse(const char* vrmlFile,char* meshFile)
{
  FILE *rFile,*wFile;
  
  fopen_s(&rFile, vrmlFile,"r");
  fopen_s(&wFile, meshFile,"w");
  parseToFile(rFile,wFile);
  
}
moTessMesh_c* vrmlParser::parseToMesh(FILE* rFile)
{
  float coordIn[3];
  parseToArr(rFile);
  moTessMesh_c* mesh=new moTessMesh_c(numOfPoints,numOfFaces);
  for(int id=0;id<numOfPoints;id++)
  {
    coordIn[0]=vertexArr[3*id];coordIn[1]=vertexArr[3*id+1];coordIn[2]=vertexArr[3*id+2];
    mesh->addVertex(coordIn);
  }
  for(int id=0;id<numOfFaces;id++)
  {
    
    mesh->addFacet(faceArr[3*id],faceArr[3*id+1],faceArr[3*id+2]);
  }
  delete [] vertexArr;
  delete [] faceArr;
  init();
  return mesh;
}
void vrmlParser::init()
{
  fsize=vsize=300;
  faceArr = new int[fsize];
  vertexArr = new float[vsize];
  numOfPoints=0;numOfFaces=0;
}
