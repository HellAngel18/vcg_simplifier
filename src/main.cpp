#include <cstdio>
#include <vcg/complex/algorithms/clean.h>

#include "glb_loader.h"
#include "mymesh.h"
#include "obj_loader.h"

// --- 主程序 ---
void LogStatus(MyMesh &m, const char *stage) { printf("[%s] V:%d F:%d\n", stage, m.VN(), m.FN()); }
void MeshLabStyleClean(MyMesh &m) {
    vcg::tri::Clean<MyMesh>::RemoveDuplicateVertex(m);
    vcg::tri::Clean<MyMesh>::RemoveDuplicateFace(m);
    vcg::tri::Clean<MyMesh>::RemoveDegenerateFace(m);
    vcg::tri::Clean<MyMesh>::RemoveZeroAreaFace(m);
    vcg::tri::Clean<MyMesh>::RemoveUnreferencedVertex(m);
    vcg::tri::Allocator<MyMesh>::CompactEveryVector(m);
}

int main(int argc, char **argv) {
    std::string inputPath;
    std::string outputPath;
    float ratio = 0.5f;

    // 简单的参数解析
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            inputPath = argv[++i];
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            ratio = float(atof(argv[++i]));
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outputPath = argv[++i];
    }

    MyMesh m;
    tinygltf::Model originalModel; // 保存原始数据（材质/纹理）

    if (inputPath.empty()) {
        printf("Please specify input file path with -i\n");
        return -1;
    }
    if (outputPath.empty()) {
        printf("Please specify output file path with -o\n");
        return -1;
    }

    if (inputPath.substr(inputPath.find_last_of('.') + 1) == "glb") {
        printf("Loading GLB %s...\n", inputPath.c_str());
        if (!LoadGLB(m, originalModel, inputPath)) {
            printf("Failed to load GLB.\n");
            return -1;
        }
        LogStatus(m, "Loaded");
    } else if (inputPath.substr(inputPath.find_last_of('.') + 1) == "obj") {
        printf("Loading OBJ %s...\n", inputPath.c_str());
        if (!LoadObj(m, inputPath)) {
            printf("Failed to load OBJ.\n");
            return -1;
        }
        LogStatus(m, "Loaded");
    } else {
        printf("Unsupported input format. Only .glb and .obj are supported.\n");
        return -1;
    }

    // 清理
    MeshLabStyleClean(m);

    // 拓扑更新
    vcg::tri::UpdateTopology<MyMesh>::FaceFace(m);
    vcg::tri::UpdateTopology<MyMesh>::VertexFace(m);
    vcg::tri::UpdateFlags<MyMesh>::FaceBorderFromVF(m);
    vcg::tri::UpdateNormal<MyMesh>::PerFaceNormalized(m);
    vcg::tri::UpdateNormal<MyMesh>::PerVertexFromCurrentFaceNormal(m);
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
    pp.PreserveBoundary  = true;
    pp.PreserveTopology  = true;
    pp.QualityThr        = 0.3;
    pp.ExtraTCoordWeight = 1.0;

    int targetFaceCount = (int)(m.fn * ratio);
    printf("Targeting %d faces\n", targetFaceCount);

    vcg::LocalOptimization<MyMesh> Deci(m, &pp);
    Deci.Init<MyCollapse>();
    Deci.SetTargetSimplices(targetFaceCount);
    Deci.DoOptimization();
    Deci.Finalize<MyCollapse>();

    vcg::tri::QuadricTexHelper<MyMesh>::TDp3() = nullptr;
    vcg::tri::QuadricTexHelper<MyMesh>::TDp()  = nullptr;

    vcg::tri::Allocator<MyMesh>::CompactEveryVector(m);
    LogStatus(m, "Final");

    if (outputPath.substr(outputPath.find_last_of('.') + 1) == "glb") {
        printf("Saving GLB %s...\n", outputPath.c_str());
        if (!SaveGLB(m, originalModel, outputPath)) {
            printf("Failed to save GLB.\n");
            return -1;
        }
    } else if (outputPath.substr(outputPath.find_last_of('.') + 1) == "obj") {
        printf("Saving OBJ %s...\n", outputPath.c_str());
        if (!SaveObj(m, outputPath)) {
            printf("Failed to save OBJ.\n");
            return -1;
        }
    } else {
        printf("Unsupported output format. Only .glb and .obj are supported.\n");
        return -1;
    }

    return 0;
}