project('experiments', 'cpp', default_options: ['cpp_std=c++20',
  'b_sanitize=address'])
omp = dependency('openmp')
executable('main', ['main.cpp', 'graph.cpp', 'experiment.cpp'],
  dependencies : omp)
