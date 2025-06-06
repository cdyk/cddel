# cddel

Implementation of Delaunay triangulation from scratch.

## Stability and precision

Point positions are represented as 32-bit unsigned integers, so scale the input points to cover this range.

The calculations are done with multi-word integer math. This allows computations to be exact, so there are no tolerances or degenerate triangles.

## Repository structure

- `src` contains the triangulation code.
- `app` contains a small SDL3-application that inserts random points into a triangulation.

## License

This software is available to anybody free of charge, under the terms of the MIT License (see LICENSE).
