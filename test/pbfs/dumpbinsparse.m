function dumpbinsparse(A, output)
% This MATLAB function dumps the matrix A to the file output.
% The result is a binary representation of A that may be used by
% bfs as an input file.

% Assuming little-endian.
f = fopen([output], 'w', 'l');

[i, j, v] = find(A);

fwrite(f, size(A,1), 'uint32');
fwrite(f, size(A,2), 'uint32');
fwrite(f, nnz(A), 'uint32');

fwrite(f, (i-1), 'uint32');
fclose(f);
clear i;

% Assuming little-endian
f = fopen([output], 'a', 'l');
fwrite(f, (j-1), 'uint32');
fclose(f);
clear j;

% Assuming little-endian
f = fopen([output], 'a', 'l');
fwrite(f, v, 'double');
fclose(f);
