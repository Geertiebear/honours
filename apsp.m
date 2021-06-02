clear variables;
close all;

A = [   0 3 0 0 1; 
        3 0 1 1 0;
        0 1 0 0 0;
        0 1 0 0 2;
        1 0 0 2 0
    ];

D = A;

for i = 1:5
    x = (D(:, i) + D(i, :));
    disp(x);
    m = min(D(i), (D(:, i) + D(i, :)));
    D = D + m;
end