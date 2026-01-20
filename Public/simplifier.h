#pragma once

#include "mymesh.h"
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/algorithms/update/topology.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <vcg/complex/algorithms/update/flag.h>

class Simplifier {
public:
    struct Params {

        // 简化比例，默认值0.5； 如果 Ratio > 0，则优先使用 Ratio 计算目标面数；否则使用 TargetFaceCount。
        float ratio = 0.5f;

        //目标面数，仅当 ratio <= 0 时生效
        int TargetFaceCount = 0;
        
        // 质量阈值，超过此代价的坍塌操作将被阻止，默认值0.3
        double qualityThr = 0.3;

        // 纹理权重，保护纹理坐标不发生扭曲，默认值1.0
        double TextureWeight = 1.0;

        // 边界保留权重，增加边界边坍塌的代价，使其更难被简化，默认值1.0
        double BoundaryWeight = 1.0;

        // 是否保护边界，如果为true，边界上的顶点将受到额外保护，默认值为true
        bool preserveBoundary = true;

        // 是否保护拓扑结构，防止简化过程中产生非流形结构，默认值为true
        bool preserveTopology = true;
        
    };

    static void Clean(MyMesh& m);
    static void Simplify(MyMesh& m, const Params& params);
};
