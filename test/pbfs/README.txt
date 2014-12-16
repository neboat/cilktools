BFS - README

The bfs program reads in a file storing a binary representation of a
graph and executes either BFS or PBFS on that graph.

***************************************************************************

LICENSING

Copyright (c) 2010, Tao B. Schardl

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***************************************************************************

USAGE

./bfs [-f <filename>] [-a <algorithm>] [-c]
Flags are:
	-f <filename>	: Specify the name of the test file to use.
	-a <algorithm>	: Specify the BFS algorithm to use.
	Valid values for <algorithm> are:
		b for Serial BFS
		p for PBFS (default)
	-c		: Check result for correctness.


***************************************************************************

COMPILATION

To compile the bfs executable, simply run:

make

You must have icc present in your path for compilation to succeed.

To cleanup the results of make, run:

make clean

***************************************************************************

GRAPH INPUT FILE: The input file to bfs is a binary file with the
following format:

<#rows><#columns><#non-zeros><row vector><column vector><logical array>

All values except for those in the logical array stored in the binary
file are unsigned 32-bit integers.  The values in the logical array
are stored as doubles.  Such a file may be generated for the graph
stored in the matrix A using the following MATLAB code*:

[i, j, v] = find(A); 

fwrite(f, size(A,1), 'uint32');
fwrite(f, size(A,2), 'uint32');
fwrite(f, nnz(A), 'uint32');

fwrite(f, (i-1), 'uint32');
fwrite(f, (j-1), 'uint32');
fwrite(f, v, 'double');

For convenience, this MATLAB code is reproduced in the included MATLAB
function "dumpbinsparse(A, output)," which outputs the matrix "A" into
the binary file "output" in the correct format.

* Thanks to Aydin Buluc for providing this MATLAB code for creating
  valid input graphs.
