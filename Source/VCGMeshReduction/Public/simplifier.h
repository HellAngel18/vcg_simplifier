#pragma once

#include "mymesh.h"
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/algorithms/update/flag.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <vcg/complex/algorithms/update/topology.h>

class Simplifier {
  public:
    struct Params {
        float ratio              = 0.5f;
        int targetFaceCount      = -1;
        bool preserveBoundary    = true;
        bool preserveTopology    = true;
        bool normalCheck         = false;
        bool optimalPlacement    = true;
        double qualityThr        = 0.3;
        double boundaryWeight    = 1.0;
        double extraTCoordWeight = 1.0;
    };

    static void Clean(MyMesh &m);
    static void Simplify(MyMesh &m, const Params &params);
};
