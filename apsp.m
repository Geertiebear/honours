clear variables;
close all;

A = [   0 3 inf inf 1; 
        3 0 1 1 inf;
        inf 1 0 inf inf;
        inf 1 inf 0 2;
        1 inf inf 2 0
    ];
G = graph(A);
plot(G);
D = A;

for k = 1:5
    for i = 1:5
        for j = 1:5
            if D(i, j) > D(i, k) + D(k, j)
                D(i, j) = D(i, k) + D(k, j);
            end
        end
    end
end