// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

extern "C++" {
#include <stdio.h>
#ifdef _WIN32
#include <tchar.h>
#endif
}
// TODO: reference additional headers your program requires here

typedef unsigned int UINT;
typedef int BOOL;
#define FALSE 0;
#define TRUE 1;
#define gcLengthTolerance 0.00000001
#define gcDoubleTolerance 100*gcLengthTolerance
#define SU_DELETE delete
#define SU_NEW new

extern "C++" {
#include <cmath>
#include <utility>
#include <vector>
//#include <minmax.h>
#include <math.h>
}
#include "vector.h"

#define do_min(x, y) ( (x) < (y) ? (x) : (y) )
#define do_max(x, y) ( (x) > (y) ? (x) : (y) )
