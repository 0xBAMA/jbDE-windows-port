This is the supplemental material of the paper

    "A Low-Dimensional Function Space for Efficient Spectral Upsampling"

The contents are as follows:

rgb2spec.{c,h}:
    Self-contained C implementation of coefficient fetch & model evaluation
    along with vectorized versions (SSE4, AVX, AVX512).

rgb2spec_test.c, Makefile:
    A small example that shows how to use the API in rgb2spec.h.

/tables/*.coeff:
    Precomputed coefficient tables for sRGB, Rec.2020, ProPhotoRGB,
    and ACES2065-1

/optimization/*
    The Jupyter notebook that was used to compute the coefficient files.
    The notebook also contains several interactive visualizations of the
    coefficient maps and resulting spectra.

    The code depends on a Python extension library that must be compiled using
    CMake. This assumes that the following dependencies are installed:

    1. Python 3.x (preferably Anaconda)
    2. CMake 3.x
    3. Glog (Google logging library, https://github.com/google/glog)
    4. Ceres solver (http://ceres-solver.org/)

    6. Afterwards, run "cmake . && make" and then launch the notebook.
