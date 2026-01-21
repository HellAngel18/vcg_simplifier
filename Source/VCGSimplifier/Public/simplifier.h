#pragma once

#include "mymesh.h"
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/algorithms/update/topology.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <vcg/complex/algorithms/update/flag.h>

class Simplifier {
public:
    struct Params {
        float ratio = 0.5f;
        bool preserveBoundary = true;
        bool preserveTopology = true;
        double qualityThr = 0.3;
        double extraTCoordWeight = 1.0;
    };

    static void Clean(MyMesh& m);
    static void Simplify(MyMesh& m, const Params& params);
};
