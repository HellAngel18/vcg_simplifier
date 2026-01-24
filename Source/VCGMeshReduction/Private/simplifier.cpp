#include "simplifier.h"
#include <vcg/complex/algorithms/local_optimization.h>

void Simplifier::Clean(MyMesh &m) {
    vcg::tri::Clean<MyMesh>::RemoveDuplicateVertex(m);
    vcg::tri::Clean<MyMesh>::RemoveDuplicateFace(m);
    vcg::tri::Clean<MyMesh>::RemoveDegenerateFace(m);
    vcg::tri::Clean<MyMesh>::RemoveZeroAreaFace(m);
    vcg::tri::Clean<MyMesh>::RemoveUnreferencedVertex(m);
    vcg::tri::Allocator<MyMesh>::CompactEveryVector(m);
}

void Simplifier::Simplify(MyMesh &m, const Params &params) {
    // 拓扑更新
    vcg::tri::UpdateTopology<MyMesh>::FaceFace(m);
    vcg::tri::UpdateTopology<MyMesh>::VertexFace(m);
    vcg::tri::UpdateFlags<MyMesh>::FaceBorderFromVF(m);
    vcg::tri::UpdateNormal<MyMesh>::PerFaceNormalized(m);
    vcg::tri::UpdateNormal<MyMesh>::PerVertex(m);
    vcg::tri::UpdateNormal<MyMesh>::NormalizePerVertex(m);

    // 简化初始化
    vcg::math::Quadric<double> QZero;
    QZero.SetZero();
    vcg::tri::QuadricTexHelper<MyMesh>::QuadricTemp TD3(m.vert, QZero);
    vcg::tri::QuadricTexHelper<MyMesh>::TDp3() = &TD3;
    std::vector<std::pair<vcg::TexCoord2<float>, vcg::Quadric5<double>>> qv;
    vcg::tri::QuadricTexHelper<MyMesh>::Quadric5Temp TD(m.vert, qv);
    vcg::tri::QuadricTexHelper<MyMesh>::TDp() = &TD;

    // 简化参数
    vcg::tri::TriEdgeCollapseQuadricTexParameter pp;
    pp.SetDefaultParams();
    pp.PreserveBoundary  = params.preserveBoundary;
    pp.PreserveTopology  = params.preserveTopology;
    pp.QualityThr        = params.qualityThr;
    pp.ExtraTCoordWeight = params.extraTCoordWeight;
    pp.BoundaryWeight    = params.boundaryWeight;
    pp.NormalCheck       = params.normalCheck;
    pp.OptimalPlacement  = params.optimalPlacement;

    int targetFaceCount;
    if (params.targetFaceCount > 0) {
        targetFaceCount = params.targetFaceCount;
    } else {
        targetFaceCount = (int)(m.fn * params.ratio);
    }

    vcg::LocalOptimization<MyMesh> Deci(m, &pp);
    Deci.Init<MyCollapse>();
    Deci.SetTargetSimplices(targetFaceCount);
    Deci.DoOptimization();
    Deci.Finalize<MyCollapse>();

    vcg::tri::QuadricTexHelper<MyMesh>::TDp3() = nullptr;
    vcg::tri::QuadricTexHelper<MyMesh>::TDp()  = nullptr;

    vcg::tri::Allocator<MyMesh>::CompactEveryVector(m);
}
