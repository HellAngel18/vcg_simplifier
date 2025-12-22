#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <cstring> // for memcpy
#include <map>     // for std::map

// --- 1. 引入 TinyGLTF ---
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h" 

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
#include <vcg/container/simple_temporary_data.h>

using namespace vcg;
using namespace std;

// --- 2. 网格定义 ---
class MyVertex; class MyEdge; class MyFace;
struct MyUsedTypes : public UsedTypes<Use<MyVertex>::AsVertexType, Use<MyEdge>::AsEdgeType, Use<MyFace>::AsFaceType> {};
class MyVertex : public Vertex<MyUsedTypes, vertex::Coord3f, vertex::Normal3f, vertex::BitFlags, vertex::Mark, vertex::VFAdj> {};
class MyFace : public Face<MyUsedTypes, face::VertexRef, face::Normal3f, face::WedgeTexCoord2f, face::BitFlags, face::Mark, face::FFAdj, face::VFAdj> {};
class MyEdge : public Edge<MyUsedTypes> {};
class MyMesh : public tri::TriMesh<std::vector<MyVertex>, std::vector<MyFace>, std::vector<MyEdge>> {};

// --- 3. 简化类定义 ---
typedef vcg::tri::BasicVertexPair<MyVertex> MyVertexPair;
class MyCollapse : public vcg::tri::TriEdgeCollapseQuadricTex<MyMesh, MyVertexPair, MyCollapse, vcg::tri::QuadricTexHelper<MyMesh>> {
public:
    using vcg::tri::TriEdgeCollapseQuadricTex<MyMesh, MyVertexPair, MyCollapse, vcg::tri::QuadricTexHelper<MyMesh>>::TriEdgeCollapseQuadricTex;
};

// --- 4. GLB 加载器 (修复版：支持多 Mesh/多 Primitive) ---
bool LoadGLB(MyMesh &m, tinygltf::Model &outModel, const std::string &filename) {
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ret = loader.LoadBinaryFromFile(&outModel, &err, &warn, filename);

    if (!warn.empty()) printf("GLB Warn: %s\n", warn.c_str());
    if (!err.empty()) printf("GLB Err: %s\n", err.c_str());
    if (!ret) return false;

    if (outModel.meshes.empty()) return false;

    // 遍历所有的 Mesh
    for (const auto &mesh : outModel.meshes) {
        // 遍历 Mesh 里的所有 Primitive (子网格)
        for (const auto &primitive : mesh.primitives) {
            
            // --- 1. 读取位置 (Position) ---
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) continue;
            
            const auto &posAcc = outModel.accessors[primitive.attributes.at("POSITION")];
            const auto &posView = outModel.bufferViews[posAcc.bufferView];
            const float *positions = reinterpret_cast<const float *>(&outModel.buffers[posView.buffer].data[posView.byteOffset + posAcc.byteOffset]);
            int posStride = posAcc.ByteStride(posView) ? (posAcc.ByteStride(posView) / 4) : 3;

            // --- 2. 读取 UV ---
            const float *texCoords = nullptr;
            int texStride = 0;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const auto &texAcc = outModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto &texView = outModel.bufferViews[texAcc.bufferView];
                texCoords = reinterpret_cast<const float *>(&outModel.buffers[texView.buffer].data[texView.byteOffset + texAcc.byteOffset]);
                texStride = texAcc.ByteStride(texView) ? (texAcc.ByteStride(texView) / 4) : 2;
            }

            // --- 3. 添加顶点 ---
            // 【关键】记录添加前的顶点数量，作为索引偏移量 (Offset)
            int vertOffset = m.VN(); 
            
            tri::Allocator<MyMesh>::AddVertices(m, posAcc.count);
            for (size_t i = 0; i < posAcc.count; ++i) {
                m.vert[vertOffset + i].P() = MyVertex::CoordType(
                    positions[i * posStride], 
                    positions[i * posStride + 1], 
                    positions[i * posStride + 2]
                );
            }

            // --- 4. 添加面 (Indices) ---
            if (primitive.indices >= 0) {
                const auto &indAcc = outModel.accessors[primitive.indices];
                const auto &indView = outModel.bufferViews[indAcc.bufferView];
                const unsigned char *dataPtr = &outModel.buffers[indView.buffer].data[indView.byteOffset + indAcc.byteOffset];
                int indStride = indAcc.ByteStride(indView);
                
                tri::Allocator<MyMesh>::AddFaces(m, indAcc.count / 3);
                int faceIdx = m.FN() - (indAcc.count / 3);

                for (size_t i = 0; i < indAcc.count; i += 3) {
                    uint32_t idx[3];
                    // 解析不同类型的索引 (unsigned short / unsigned int / unsigned byte)
                    for(int k=0; k<3; ++k) {
                        size_t offset = (i + k);
                        if (indAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            idx[k] = *reinterpret_cast<const uint16_t*>(dataPtr + offset * (indStride?indStride:2));
                        } else if (indAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                            idx[k] = *reinterpret_cast<const uint32_t*>(dataPtr + offset * (indStride?indStride:4));
                        } else if (indAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            idx[k] = *reinterpret_cast<const uint8_t*>(dataPtr + offset * (indStride?indStride:1));
                        } else {
                            idx[k] = 0; // Should not happen
                        }
                    }

                    // 【关键】加上 vertOffset，因为我们是追加到同一个网格里
                    m.face[faceIdx].V(0) = &m.vert[vertOffset + idx[0]];
                    m.face[faceIdx].V(1) = &m.vert[vertOffset + idx[1]];
                    m.face[faceIdx].V(2) = &m.vert[vertOffset + idx[2]];

                    if (texCoords) {
                        m.face[faceIdx].WT(0) = MyFace::TexCoordType(texCoords[idx[0] * texStride], texCoords[idx[0] * texStride + 1]);
                        m.face[faceIdx].WT(1) = MyFace::TexCoordType(texCoords[idx[1] * texStride], texCoords[idx[1] * texStride + 1]);
                        m.face[faceIdx].WT(2) = MyFace::TexCoordType(texCoords[idx[2] * texStride], texCoords[idx[2] * texStride + 1]);
                    }
                    faceIdx++;
                }
            }
        }
    }
    return true;
}

// --- 5. GLB 保存器 (已修复编译错误与MeshLab兼容性) ---
bool SaveGLB(MyMesh &m, const tinygltf::Model &originalModel, const std::string &filename) {
    tinygltf::Model outModel;
    
    // 1. 复制配置
    outModel.asset = originalModel.asset;
    outModel.asset.generator = "VCG-Simplifier";
    
    // 【关键修复】处理图片和纹理引用
    outModel.materials = originalModel.materials;
    outModel.textures = originalModel.textures;
    outModel.samplers = originalModel.samplers;
    outModel.images = originalModel.images;

    // 清除 BufferView 索引，强制 TinyGLTF 重新打包图片数据
    for (auto &img : outModel.images) {
        img.bufferView = -1; 
        img.uri = ""; 
    }

    // 2. 准备几何数据容器
    std::vector<float> rawPos;
    std::vector<float> rawNor;
    std::vector<float> rawUV;
    std::vector<unsigned int> rawIndices;

    // 用于裂解顶点 (Wedge UV -> Vertex Attributes)
    struct VertKey {
        int vIdx;
        float u, v;
        bool operator<(const VertKey& o) const {
            if (vIdx != o.vIdx) return vIdx < o.vIdx;
            if (u != o.u) return u < o.u;
            return v < o.v;
        }
    };
    std::map<VertKey, unsigned int> uniqueMap;
    int idxCounter = 0;

    // 【关键修复】计算包围盒 (Min/Max)，MeshLab 必须需要这个
    std::vector<double> posMin = {1e9, 1e9, 1e9};
    std::vector<double> posMax = {-1e9, -1e9, -1e9};

    for(auto &f : m.face) {
        if(f.IsD()) continue;
        for(int i=0; i<3; ++i) {
            int vIdx = vcg::tri::Index(m, f.V(i));
            float u = f.WT(i).u();
            float v = f.WT(i).v();
            
            VertKey key = {vIdx, u, v};
            auto it = uniqueMap.find(key);
            
            if(it != uniqueMap.end()) {
                rawIndices.push_back(it->second);
            } else {
                uniqueMap[key] = idxCounter;
                rawIndices.push_back(idxCounter);
                
                float px = f.V(i)->P().X();
                float py = f.V(i)->P().Y();
                float pz = f.V(i)->P().Z();

                rawPos.push_back(px);
                rawPos.push_back(py);
                rawPos.push_back(pz);
                
                // 更新 Min/Max
                if (px < posMin[0]) posMin[0] = px; if (px > posMax[0]) posMax[0] = px;
                if (py < posMin[1]) posMin[1] = py; if (py > posMax[1]) posMax[1] = py;
                if (pz < posMin[2]) posMin[2] = pz; if (pz > posMax[2]) posMax[2] = pz;
                
                rawNor.push_back(f.V(i)->N().X());
                rawNor.push_back(f.V(i)->N().Y());
                rawNor.push_back(f.V(i)->N().Z());
                
                rawUV.push_back(u);
                rawUV.push_back(v);
                
                idxCounter++;
            }
        }
    }

    // 3. 构建二进制 Buffer
    tinygltf::Buffer buffer;
    
    size_t lenPos = rawPos.size() * sizeof(float);
    size_t lenNor = rawNor.size() * sizeof(float);
    size_t lenUV  = rawUV.size()  * sizeof(float);
    size_t lenInd = rawIndices.size() * sizeof(unsigned int);

    size_t offsetPos = 0;
    size_t offsetNor = offsetPos + lenPos;
    size_t offsetUV  = offsetNor + lenNor;
    size_t offsetInd = offsetUV  + lenUV;
    
    size_t totalSize = offsetInd + lenInd;
    
    // 【关键修复】Buffer 总长度必须是 4 的倍数 (Padding)
    size_t padding = 0;
    if (totalSize % 4 != 0) {
        padding = 4 - (totalSize % 4);
    }

    buffer.data.resize(totalSize + padding);
    
    // 写入数据
    std::memcpy(buffer.data.data() + offsetPos, rawPos.data(), lenPos);
    std::memcpy(buffer.data.data() + offsetNor, rawNor.data(), lenNor);
    std::memcpy(buffer.data.data() + offsetUV,  rawUV.data(), lenUV);
    std::memcpy(buffer.data.data() + offsetInd, rawIndices.data(), lenInd);
    
    outModel.buffers.push_back(buffer);
    int bufferId = 0;

    // 4. 创建 BufferViews
    auto addBufferView = [&](size_t offset, size_t length, int target) {
        tinygltf::BufferView bv;
        bv.buffer = bufferId;
        bv.byteOffset = offset;
        bv.byteLength = length;
        bv.target = target;
        outModel.bufferViews.push_back(bv);
        return (int)outModel.bufferViews.size() - 1;
    };

    int bvPos = addBufferView(offsetPos, lenPos, TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvNor = addBufferView(offsetNor, lenNor, TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvUV  = addBufferView(offsetUV,  lenUV,  TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvInd = addBufferView(offsetInd, lenInd, TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

    // 5. 创建 Accessors
    // 【关键修复】参数 typeEnum 改为 int，并接收 Min/Max
    auto addAccessor = [&](int bv, int count, int compType, int typeEnum, const std::vector<double>* minV, const std::vector<double>* maxV) {
        tinygltf::Accessor acc;
        acc.bufferView = bv;
        acc.byteOffset = 0;
        acc.componentType = compType; 
        acc.count = count;
        acc.type = typeEnum; // 这里使用 TINYGLTF_TYPE_*
        if (minV) acc.minValues = *minV;
        if (maxV) acc.maxValues = *maxV;
        outModel.accessors.push_back(acc);
        return (int)outModel.accessors.size() - 1;
    };

    // Position 必须要有 Min/Max
    int accPos = addAccessor(bvPos, rawPos.size()/3, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, &posMin, &posMax);
    int accNor = addAccessor(bvNor, rawNor.size()/3, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, nullptr, nullptr);
    int accUV  = addAccessor(bvUV,  rawUV.size()/2,  TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC2, nullptr, nullptr);
    int accInd = addAccessor(bvInd, rawIndices.size(), TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, TINYGLTF_TYPE_SCALAR, nullptr, nullptr);

    // 6. 组装 Mesh
    tinygltf::Mesh mesh;
    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = accPos;
    prim.attributes["NORMAL"] = accNor;
    prim.attributes["TEXCOORD_0"] = accUV;
    prim.indices = accInd;
    prim.mode = TINYGLTF_MODE_TRIANGLES;
    
    // 恢复材质引用
    if (!originalModel.materials.empty()) {
        prim.material = 0; 
    }

    mesh.primitives.push_back(prim);
    outModel.meshes.push_back(mesh);

    // 7. Node & Scene
    tinygltf::Node node;
    node.mesh = 0;
    outModel.nodes.push_back(node);

    tinygltf::Scene scene;
    scene.nodes.push_back(0);
    outModel.scenes.push_back(scene);
    outModel.defaultScene = 0;

    // 8. 写入文件
    tinygltf::TinyGLTF loader;
    // 参数含义: Model, filename, embedImages, embedBuffers, prettyPrint, writeBinary
    // 注意：embedBuffers = true 是关键
    return loader.WriteGltfSceneToFile(&outModel, filename, true, true, true, true); 
}

// --- 主程序 ---
void LogStatus(MyMesh &m, const char *stage) { printf("[%s] V:%d F:%d\n", stage, m.VN(), m.FN()); }
void MeshLabStyleClean(MyMesh &m) {
    tri::Clean<MyMesh>::RemoveDuplicateVertex(m);
    tri::Clean<MyMesh>::RemoveDuplicateFace(m);
    tri::Clean<MyMesh>::RemoveDegenerateFace(m);
    tri::Clean<MyMesh>::RemoveZeroAreaFace(m);
    tri::Clean<MyMesh>::RemoveUnreferencedVertex(m);
    tri::Allocator<MyMesh>::CompactEveryVector(m);
}

int main(int argc, char **argv) {
    std::string inputPath = "input.glb";
    std::string outputPath = "out.glb";
    float ratio = 0.5f;

    // 简单的参数解析
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) inputPath = argv[++i];
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) ratio = float(atof(argv[++i]));
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outputPath = argv[++i];
    }

    MyMesh m;
    tinygltf::Model originalModel; // 保存原始数据（材质/纹理）

    printf("Loading GLB %s...\n", inputPath.c_str());
    if (!LoadGLB(m, originalModel, inputPath)) {
        printf("Failed to load GLB.\n");
        return -1;
    }
    LogStatus(m, "Loaded");

    // 清理
    MeshLabStyleClean(m);

    // 拓扑更新
    tri::UpdateTopology<MyMesh>::FaceFace(m);
    tri::UpdateTopology<MyMesh>::VertexFace(m);
    tri::UpdateFlags<MyMesh>::FaceBorderFromVF(m);
    tri::UpdateNormal<MyMesh>::PerFaceNormalized(m);
    tri::UpdateNormal<MyMesh>::PerVertexFromCurrentFaceNormal(m);
    tri::UpdateNormal<MyMesh>::NormalizePerVertex(m);

    // 简化初始化
    math::Quadric<double> QZero; QZero.SetZero();
    tri::QuadricTexHelper<MyMesh>::QuadricTemp TD3(m.vert, QZero);
    tri::QuadricTexHelper<MyMesh>::TDp3() = &TD3;
    std::vector<std::pair<vcg::TexCoord2<float>, Quadric5<double>>> qv;
    tri::QuadricTexHelper<MyMesh>::Quadric5Temp TD(m.vert, qv);
    tri::QuadricTexHelper<MyMesh>::TDp() = &TD;

    // 简化参数
    vcg::tri::TriEdgeCollapseQuadricTexParameter pp;
    pp.SetDefaultParams();
    pp.PreserveBoundary = true;
    pp.PreserveTopology = true;
    pp.QualityThr = 0.3;
    pp.ExtraTCoordWeight = 1.0; 

    int targetFaceCount = (int)(m.fn * ratio);
    printf("Targeting %d faces\n", targetFaceCount);

    vcg::LocalOptimization<MyMesh> Deci(m, &pp);
    Deci.Init<MyCollapse>();
    Deci.SetTargetSimplices(targetFaceCount);
    Deci.DoOptimization();
    Deci.Finalize<MyCollapse>();

    tri::QuadricTexHelper<MyMesh>::TDp3() = nullptr;
    tri::QuadricTexHelper<MyMesh>::TDp() = nullptr;

    tri::Allocator<MyMesh>::CompactEveryVector(m);
    LogStatus(m, "Final");

    // 保存 GLB
    printf("Saving GLB to %s...\n", outputPath.c_str());
    if (SaveGLB(m, originalModel, outputPath)) {
        printf("Success.\n");
    } else {
        printf("Failed to save GLB.\n");
    }

    return 0;
}