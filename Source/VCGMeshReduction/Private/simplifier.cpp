#include "simplifier.h"
#include <vcg/complex/algorithms/local_optimization.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric_tex.h>

void Simplifier::Clean(MyMesh &m) {
    vcg::tri::Clean<MyMesh>::RemoveDuplicateVertex(m);
    vcg::tri::Clean<MyMesh>::RemoveDuplicateFace(m);
    vcg::tri::Clean<MyMesh>::RemoveDegenerateFace(m);
    vcg::tri::Clean<MyMesh>::RemoveZeroAreaFace(m);
    vcg::tri::Clean<MyMesh>::RemoveUnreferencedVertex(m);
    vcg::tri::Allocator<MyMesh>::CompactEveryVector(m);
}

void Simplifier::Simplify(MyMesh &m, const Params &params) {
    // Preprocess
    vcg::tri::UpdateTopology<MyMesh>::VertexFace(m);
    vcg::tri::UpdateTopology<MyMesh>::FaceFace(m);

    vcg::tri::UpdateFlags<MyMesh>::FaceBorderFromVF(m);
    // 简化
    vcg::tri::UpdateNormal<MyMesh>::PerFace(m);
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

    int targetCount = params.targetFaceCount;
    if (targetCount < 0) {
        targetCount = (int)(m.fn * params.ratio);
    }

    vcg::LocalOptimization<MyMesh> DeciSession(m, &pp);
    DeciSession.Init<MyCollapse>();
    DeciSession.SetTargetSimplices(targetCount);
    DeciSession.SetTimeBudget(0.1f);

    while (DeciSession.DoOptimization() && m.fn > targetCount) {
        // 可以在这里添加进度更新的回调
    }
    DeciSession.Finalize<MyCollapse>();
    vcg::tri::QuadricTexHelper<MyMesh>::TDp()  = nullptr;
    vcg::tri::QuadricTexHelper<MyMesh>::TDp3() = nullptr;
    // 更新法线
    vcg::tri::UpdateBounding<MyMesh>::Box(m);
    if (m.fn > 0) {
        vcg::tri::UpdateNormal<MyMesh>::PerFaceNormalized(m);
        vcg::tri::UpdateNormal<MyMesh>::PerVertexAngleWeighted(m);
    }
    vcg::tri::UpdateNormal<MyMesh>::NormalizePerFace(m);
    vcg::tri::UpdateNormal<MyMesh>::PerVertexFromCurrentFaceNormal(m);
    vcg::tri::UpdateNormal<MyMesh>::NormalizePerVertex(m);
}
