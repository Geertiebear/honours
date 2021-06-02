clear variables;
close all;

A = [   0 1 1 1 1; 
        1 0 1 1 1;
        1 1 0 1 1;
        1 1 1 0 1;
        1 1 1 1 0
    ];
G = graph(A);
plot(G);

B = A * [0; 1; 0; 0; 0;];