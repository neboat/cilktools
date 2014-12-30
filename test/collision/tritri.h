//$c2 KDH 04/25/07 support for new tri tri intersection code
//$c1   AKP 08/29/06 Created 
//========================================================================//
//              Copyright 2006 (Unpublished Material)                     //
//                  SolidWorks Corporation.	                          //
//========================================================================//
//
//     File Name:	tritri.h
//     
//     Application:	Triangle Triangle Intersection
//     
//     Contents: Utility to detect intersection between two 3D triangles
//
//========================================================================//

// 3D Triangle/triangle intersection test routine,
// See article "A Fast Triangle-Triangle Intersection Test",
// Journal of Graphics Tools, 2(2), 1997
//
// parameters: vertices of triangle 1: v0, v1, v2
//             vertices of triangle 2: u0, u1, u2
//             It is NOT required for the vertices to be in CCW order
// result    : returns true if the triangles intersect, otherwise false
//
extern bool moTriTriIntersect(
						double v0[3], double v1[3], double v2[3],
						double u0[3], double u1[3], double u2[3], bool ignore2D );

// 3D coplanar triangle/triangle intersection test routine,
// Projects the triangles onto a plane that maximizes their area and
// uses moTriTriIntersect2D to test the intersection
//
// parameters: vertices of triangle 1: v0, v1, v2
//             vertices of triangle 2: u0, u1, u2
//             It is NOT required for the vertices to be in CCW order
// result    : returns true if the triangles intersect, otherwise false
//
extern bool moTriTriIntersectCoplanar(
						double v0[3], double v1[3], double v2[3],
						double u0[3], double u1[3], double u2[3] );

// 2D triangle/triangle intersection test routine,
// Uses "Method of Separating Axis"
// See section 7.7 of "Geometric Tools for Computer Graphics",
// by Philip J. Schneider and Davis H. Eberly
//
// parameters: vertices of triangle 1: v0, v1, v2
//             vertices of triangle 2: u0, u1, u2
//             It is NOT required for the vertices to be in CCW order
// result    : returns true if the triangles intersect, otherwise false
//
extern bool moTriTriIntersect2D(
						double v0[2], double v1[2], double v2[2],
						double u0[2], double u1[2], double u2[2] );
