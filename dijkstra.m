clear variables;
close all;

A = [   0 3 0 0 1; 
        3 0 1 1 0;
        0 1 0 0 0;
        0 1 0 0 2;
        1 0 0 2 0
    ];
G = graph(A);
plot(G);

y = A' * [0; 1; 0; 0; 0;];
z = A' * [1; 1; 1; 1; 1;];

B = zeros(1, 5);
B(B == 0) = inf;
D = zeros(1, 5);

target_node = 5;
B(2) = 0;
while 1
    avec = ones(5, 1);
    % matrix * vector
    all = A' * avec;
    disp(all);
    if ~any(all)
        break;
    end
    % min value operation (with AND condition)
    [dist, idx] = min_with_condition(B, all);
    
    nvec = zeros(5, 1);
    nvec(idx) = 1;
    
    % matrix * vec
    neighbours = A' * nvec;
    neighbours(neighbours == 0) = inf;
    
    % matrix write
    A = remove_node(A, idx);
    
    % Might be the most difficult to optimise correctly
    for i = 1:length(B)
        if B(idx) + neighbours(i) < B(i)
            B(i) = B(idx) + neighbours(i);
            D(i) = idx;
        end
    end
end

function [m, idx] = min_with_condition(vec, non_zero_vec)
    m = inf;
    idx = 0;
    for i = 1:length(vec)
        if (vec(i) < m) && non_zero_vec(i)
            m = vec(i);
            idx = i;
        end
    end
end

function m = remove_node(m, idx)
    m(idx, :) = 0;
    for i = 1:length(m(idx, :))
        m(i, idx) = 0;
    end
end