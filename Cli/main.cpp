#include <cstdio>
#include "simplifier.h"
#include "glb_loader.h"
#include "mymesh.h"
#include "obj_loader.h"

// --- 主程序 ---
void LogStatus(MyMesh &m, const char *stage) { printf("[%s] V:%d F:%d\n", stage, m.VN(), m.FN()); }

void PrintUsage(const char* progName) {
    printf("Usage: %s -i <input> -o <output> [options]\n", progName);
    printf("Options:\n");
    printf("  -r  <float>   Simplification Ratio (default: 0.5). If > 0, overrides target count.\n");
    printf("  -tc <int>     Target Face Count (default: 0). Used if ratio <= 0.\n");
    printf("  -q  <float>   Quality Threshold (default: 0.3).\n");
    printf("  -tw <float>   Texture Weight (default: 1.0).\n");
    printf("  -bw <float>   Boundary Weight (default: 1.0).\n");
    printf("  -pb <0/1>     Preserve Boundary (default: 1).\n");
    printf("  -pt <0/1>     Preserve Topology (default: 1).\n");
}

int main(int argc, char **argv) {
    std::string inputPath;
    std::string outputPath;
    
    // 初始化参数为默认值
    Simplifier::Params params;

    // 简单的参数解析
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            inputPath = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outputPath = argv[++i];
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            params.ratio = float(atof(argv[++i]));
        else if (strcmp(argv[i], "-tc") == 0 && i + 1 < argc)
            params.TargetFaceCount = atoi(argv[++i]);
        else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc)
            params.qualityThr = atof(argv[++i]);
        else if (strcmp(argv[i], "-tw") == 0 && i + 1 < argc)
            params.TextureWeight = atof(argv[++i]);
        else if (strcmp(argv[i], "-bw") == 0 && i + 1 < argc)
            params.BoundaryWeight = atof(argv[++i]);
        else if (strcmp(argv[i], "-pb") == 0 && i + 1 < argc)
            params.preserveBoundary = (atoi(argv[++i]) != 0);
        else if (strcmp(argv[i], "-pt") == 0 && i + 1 < argc)
            params.preserveTopology = (atoi(argv[++i]) != 0);
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
    Simplifier::Clean(m);

    // 简化
    Simplifier::Simplify(m, params);

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