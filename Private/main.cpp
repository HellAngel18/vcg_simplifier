#include <cstdio>
#include "simplifier.h"
#include "glb_loader.h"
#include "mymesh.h"
#include "obj_loader.h"

// --- 主程序 ---
void LogStatus(MyMesh &m, const char *stage) { printf("[%s] V:%d F:%d\n", stage, m.VN(), m.FN()); }

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
    Simplifier::Clean(m);

    // 简化
    Simplifier::Params params;
    params.ratio = ratio;
    printf("Targeting %d faces\n", (int)(m.fn * ratio));
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