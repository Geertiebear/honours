# Simulator for DATE23-SparseMEM paper

This repository contains the code for the DATE23 SparseMEM paper. The simulator
is authored by Geert Custers. The simulator runs a multithreaded simulation of
the crossbar setups proposed in GraphR[^1] and SparseMEM.

[^1] [arXiv:1708.06248](https://arxiv.org/abs/1708.06248)

## How to run

Run the following commands:

```
mkdir build
meson build
cd build
ninja
```

The build system will output a binary called ``main``, which takes as argument a
path to a file describing a graph dataset. The simulator is equipped to parse a
dataset format where each edge is described as ``<src> <dest> <weight>``.
