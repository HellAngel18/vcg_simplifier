#include <cmath>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

// VCG Headers
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/algorithms/local_optimization.h>
#include <vcg/complex/algorithms/update/bounding.h>
#include <vcg/complex/algorithms/update/flag.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <vcg/complex/algorithms/update/topology.h>
#include <vcg/complex/complex.h>

// Algorithms
#include <vcg/complex/algorithms/edge_collapse.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric_tex.h>

// 引入 SimpleTempData 用于管理临时数据 [MeshLab 关键依赖]
#include <vcg/container/simple_temporary_data.h>

// IO
#include <wrap/io_trimesh/export.h>
#include <wrap/io_trimesh/import.h>

using namespace vcg;
using namespace std;

// --- 1. 网格定义 ---

class MyVertex;
class MyEdge;
class MyFace;

struct MyUsedTypes : public UsedTypes<Use<MyVertex>::AsVertexType, Use<MyEdge>::AsEdgeType,
                                      Use<MyFace>::AsFaceType> {};

class MyVertex : public Vertex<MyUsedTypes, vertex::Coord3f, vertex::Normal3f, vertex::BitFlags,
                               vertex::Mark, vertex::VFAdj> {};

class MyFace : public Face<MyUsedTypes, face::VertexRef, face::Normal3f, face::WedgeTexCoord2f,
                           face::BitFlags, face::Mark, face::FFAdj, face::VFAdj> {};

class MyEdge : public Edge<MyUsedTypes> {};
class MyMesh
    : public tri::TriMesh<std::vector<MyVertex>, std::vector<MyFace>, std::vector<MyEdge>> {};

// --- 2. 简化策略 (带纹理) ---

typedef vcg::tri::BasicVertexPair<MyVertex> MyVertexPair;

class MyCollapse;

// 使用带纹理支持的简化类
class MyCollapse
    : public vcg::tri::TriEdgeCollapseQuadricTex<MyMesh, MyVertexPair, MyCollapse,
                                                 vcg::tri::QuadricTexHelper<MyMesh>> { // 注意这里的
                                                                                       // Helper
  public:
    using vcg::tri::TriEdgeCollapseQuadricTex<
        MyMesh, MyVertexPair, MyCollapse,
        vcg::tri::QuadricTexHelper<MyMesh>>::TriEdgeCollapseQuadricTex;
};

// --- 3. 辅助函数 ---

void LogStatus(MyMesh &m, const char *stage) { printf("[%s] V:%d F:%d\n", stage, m.VN(), m.FN()); }

// MeshLab 风格的清理
void MeshLabStyleClean(MyMesh &m) {
    printf("--- Starting Deep Cleaning ---\n");
    // 基础清理
    int dupV   = tri::Clean<MyMesh>::RemoveDuplicateVertex(m);
    int dupF   = tri::Clean<MyMesh>::RemoveDuplicateFace(m);
    int degF   = tri::Clean<MyMesh>::RemoveDegenerateFace(m); // 拓扑退化
    int unrefV = tri::Clean<MyMesh>::RemoveUnreferencedVertex(m);

    // 关键：移除零面积面 (几何退化)，这是导致 NaN 的元凶之一
    int zeroF = tri::Clean<MyMesh>::RemoveZeroAreaFace(m);

    printf("Removed: %d dupV, %d dupF, %d degF, %d zeroF, %d unrefV\n", dupV, dupF, degF, zeroF,
           unrefV);

    // 内存整理
    tri::Allocator<MyMesh>::CompactEveryVector(m);
}

// 检查是否包含有效的纹理坐标
bool CheckTexCoords(MyMesh &m) {
    if (!tri::HasPerWedgeTexCoord(m))
        return false;
    // 简单检查是否有 NaN
    for (auto &f : m.face) {
        if (f.IsD())
            continue;
        for (int i = 0; i < 3; ++i) {
            if (std::isnan(f.WT(i).u()) || std::isnan(f.WT(i).v()))
                return false;
        }
    }
    return true;
}

// --- 主程序 ---

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: vcg-simplifier -i <input.obj> [-r <ratio 0.0-1.0>] [-o <output.obj>]\n");
        return 1;
    }

    std::string inputPath, outputPath = "out.obj";
    float ratio = 0.5f;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            inputPath = argv[++i];
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            ratio = float(atof(argv[++i]));
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outputPath = argv[++i];
    }

    string pathStr   = inputPath;
    size_t lastSlash = pathStr.rfind('/');
    if (lastSlash != string::npos)
        chdir(pathStr.substr(0, lastSlash).c_str());

    MyMesh m;

    printf("Loading %s...\n", inputPath.c_str());
    int loadMask = tri::io::Mask::IOM_WEDGTEXCOORD;
    int err      = tri::io::Importer<MyMesh>::Open(m, inputPath.c_str(), loadMask);

    if (err) {
        printf("Error: %s\n", tri::io::Importer<MyMesh>::ErrorMsg(err));
        if (tri::io::Importer<MyMesh>::ErrorCritical(err))
            return -1;
    }

    LogStatus(m, "Loaded");

    // 1. 清理
    MeshLabStyleClean(m);

    // 2. 拓扑与法线
    printf("Updating topology...\n");
    tri::UpdateTopology<MyMesh>::FaceFace(m);
    tri::UpdateTopology<MyMesh>::VertexFace(m); // 必须建立 VF 拓扑

    // MeshLab 这里使用的是 FaceBorderFromVF
    tri::UpdateFlags<MyMesh>::FaceBorderFromVF(m);
    // 更新法线
    tri::UpdateNormal<MyMesh>::PerFaceNormalized(m);
    tri::UpdateNormal<MyMesh>::PerVertexFromCurrentFaceNormal(m);
    tri::UpdateNormal<MyMesh>::NormalizePerVertex(m);

    // 3. 准备简化所需的数据结构
    // =========================================================
    // 【关键修复】这里是 MeshLab 不会崩而你的代码会崩的核心原因
    // =========================================================
    printf("Initializing Quadric Temp Data...\n");

    // 初始化全零的几何 Quadric
    math::Quadric<double> QZero;
    QZero.SetZero();

    // 分配临时数据：几何 Quadric
    tri::QuadricTexHelper<MyMesh>::QuadricTemp TD3(m.vert, QZero);
    // 绑定静态指针，让 Helper 能找到这些数据
    tri::QuadricTexHelper<MyMesh>::TDp3() = &TD3;

    // 分配临时数据：纹理 Quadric (5维)
    std::vector<std::pair<vcg::TexCoord2<float>, Quadric5<double>>> qv;
    tri::QuadricTexHelper<MyMesh>::Quadric5Temp TD(m.vert, qv);
    // 绑定静态指针
    tri::QuadricTexHelper<MyMesh>::TDp() = &TD;
    // =========================================================

    // 4. 设置参数
    vcg::tri::TriEdgeCollapseQuadricTexParameter pp;
    pp.SetDefaultParams();
    pp.PreserveBoundary = true;
    pp.PreserveTopology = true;
    pp.QualityThr       = 0.3;
    pp.NormalThrRad     = M_PI / 2.0;

    // 如果没有纹理，关掉纹理权重
    if (!CheckTexCoords(m)) {
        printf("Warning: No valid texture coordinates found. Disabling texture optimization.\n");
        pp.ExtraTCoordWeight = 0.0;
    } else {
        pp.ExtraTCoordWeight = 1.0;
    }

    int targetFaceCount = (int)(m.fn * ratio);
    printf("Targeting %d faces\n", targetFaceCount);

    // 5. 执行简化
    vcg::LocalOptimization<MyMesh> Deci(m, &pp);

    printf("Initializing heap...\n");
    // 这里的 Init 会调用 Helper 来访问上面初始化的 TD 和 TD3
    // 如果没有上面的初始化，这里就会 Segfault
    Deci.Init<MyCollapse>();

    Deci.SetTargetSimplices(targetFaceCount);

    printf("Simplifying...\n");
    Deci.DoOptimization();

    // 结束简化
    Deci.Finalize<MyCollapse>();

    // =========================================================
    // 【关键】使用完后将静态指针置空，防止野指针
    tri::QuadricTexHelper<MyMesh>::TDp3() = nullptr;
    tri::QuadricTexHelper<MyMesh>::TDp()  = nullptr;
    // =========================================================

    // 6. 保存
    tri::Allocator<MyMesh>::CompactEveryVector(m);
    LogStatus(m, "Final");

    printf("Saving to %s...\n", outputPath.c_str());
    tri::io::Exporter<MyMesh>::Save(m, outputPath.c_str(), tri::io::Mask::IOM_WEDGTEXCOORD);

    printf("Done.\n");
    return 0;
}