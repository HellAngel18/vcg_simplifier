#include "glb_loader.h"

#include "mymesh.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

// --- 4. GLB 加载器 (修复版：预分配内存避免指针失效) ---
bool LoadGLB(MyMesh &m, tinygltf::Model &outModel, const std::string &filename) {
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ret = loader.LoadBinaryFromFile(&outModel, &err, &warn, filename);

    if (!warn.empty())
        printf("GLB Warn: %s\n", warn.c_str());
    if (!err.empty())
        printf("GLB Err: %s\n", err.c_str());
    if (!ret)
        return false;

    if (outModel.meshes.empty())
        return false;

    // --- Phase 1: Count total vertices and faces ---
    int totalVertices = 0;
    int totalFaces    = 0;

    for (const auto &mesh : outModel.meshes) {
        for (const auto &primitive : mesh.primitives) {
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const auto &posAcc = outModel.accessors[primitive.attributes.at("POSITION")];
                totalVertices += posAcc.count;
            }
            if (primitive.indices >= 0) {
                const auto &indAcc = outModel.accessors[primitive.indices];
                totalFaces += indAcc.count / 3;
            }
        }
    }

    printf("LoadGLB: Pre-allocating %d vertices and %d faces.\n", totalVertices, totalFaces);
    m.Clear();
    vcg::tri::Allocator<MyMesh>::AddVertices(m, totalVertices);
    vcg::tri::Allocator<MyMesh>::AddFaces(m, totalFaces);

    // --- Phase 2: Fill data ---
    int globalVertOffset = 0;
    int globalFaceOffset = 0;

    // 遍历所有的 Mesh
    int meshCount = 0;
    for (const auto &mesh : outModel.meshes) {
        printf("Loading Mesh %d with %d primitives\n", meshCount++, (int)mesh.primitives.size());
        int primIdx = 0;
        // 遍历 Mesh 里的所有 Primitive (子网格)
        for (const auto &primitive : mesh.primitives) {

            // --- 1. 读取位置 (Position) ---
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
                printf("  [Warning] Skip Primitive: No POSITION attribute found.\n");
                continue;
            }

            const auto &posAcc     = outModel.accessors[primitive.attributes.at("POSITION")];
            const auto &posView    = outModel.bufferViews[posAcc.bufferView];
            const float *positions = reinterpret_cast<const float *>(
                &outModel.buffers[posView.buffer].data[posView.byteOffset + posAcc.byteOffset]);
            int posStride = posAcc.ByteStride(posView) ? (posAcc.ByteStride(posView) / 4) : 3;

            // --- 2. 读取 UV ---
            const float *texCoords = nullptr;
            int texStride          = 0;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const auto &texAcc  = outModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto &texView = outModel.bufferViews[texAcc.bufferView];
                texCoords           = reinterpret_cast<const float *>(
                    &outModel.buffers[texView.buffer].data[texView.byteOffset + texAcc.byteOffset]);
                texStride = texAcc.ByteStride(texView) ? (texAcc.ByteStride(texView) / 4) : 2;
            }

            // --- 2.1 读取颜色 (Color) ---
            const void *colors = nullptr; // 可以是 float 或 u8/u16
            int colorCompType  = TINYGLTF_COMPONENT_TYPE_FLOAT;
            int colorType      = TINYGLTF_TYPE_VEC4;
            int colorStride    = 0;

            if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
                const auto &colAcc  = outModel.accessors[primitive.attributes.at("COLOR_0")];
                const auto &colView = outModel.bufferViews[colAcc.bufferView];
                colors =
                    &outModel.buffers[colView.buffer].data[colView.byteOffset + colAcc.byteOffset];
                colorCompType = colAcc.componentType;
                colorType     = colAcc.type;
                colorStride   = colAcc.ByteStride(colView);
            }

            // --- 3. 填充顶点 ---
            // 使用 globalVertOffset
            int currentPrimVertCount = posAcc.count;

            for (size_t i = 0; i < currentPrimVertCount; ++i) {
                m.vert[globalVertOffset + i].P() =
                    MyVertex::CoordType(positions[i * posStride], positions[i * posStride + 1],
                                        positions[i * posStride + 2]);

                // 处理颜色
                if (colors) {
                    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
                    size_t offset = i * (colorStride ? colorStride : 0); // Stride 处理

                    if (colorCompType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        const float *c =
                            (const float *)((const char *)colors +
                                            (colorStride
                                                 ? offset
                                                 : i * (colorType == TINYGLTF_TYPE_VEC4 ? 16
                                                                                        : 12)));
                        r = c[0];
                        g = c[1];
                        b = c[2];
                        if (colorType == TINYGLTF_TYPE_VEC4)
                            a = c[3];
                    } else if (colorCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        // Normalized USHORT
                        const unsigned short *c =
                            (const unsigned short *)((const char *)colors +
                                                     (colorStride
                                                          ? offset
                                                          : i * (colorType == TINYGLTF_TYPE_VEC4
                                                                     ? 8
                                                                     : 6)));
                        r = c[0] / 65535.0f;
                        g = c[1] / 65535.0f;
                        b = c[2] / 65535.0f;
                        if (colorType == TINYGLTF_TYPE_VEC4)
                            a = c[3] / 65535.0f;
                    } else if (colorCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        // Normalized UBYTE
                        const unsigned char *c =
                            (const unsigned char *)((const char *)colors +
                                                    (colorStride
                                                         ? offset
                                                         : i * (colorType == TINYGLTF_TYPE_VEC4
                                                                    ? 4
                                                                    : 3)));
                        r = c[0] / 255.0f;
                        g = c[1] / 255.0f;
                        b = c[2] / 255.0f;
                        if (colorType == TINYGLTF_TYPE_VEC4)
                            a = c[3] / 255.0f;
                    }

                    m.vert[globalVertOffset + i].C() =
                        vcg::Color4b((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255),
                                     (uint8_t)(a * 255));
                } else {
                    m.vert[globalVertOffset + i].C() = vcg::Color4b::White;
                }
            }

            // --- 4. 填充面 (Indices) ---
            if (primitive.indices >= 0) {
                const auto &indAcc  = outModel.accessors[primitive.indices];
                const auto &indView = outModel.bufferViews[indAcc.bufferView];
                const unsigned char *dataPtr =
                    &outModel.buffers[indView.buffer].data[indView.byteOffset + indAcc.byteOffset];
                int indStride = indAcc.ByteStride(indView);

                int currentPrimFaceCount = indAcc.count / 3;
                // 注意：这里不调用 AddFaces，而是直接使用 globalFaceOffset

                for (size_t i = 0; i < indAcc.count; i += 3) {
                    uint32_t idx[3];
                    // 解析不同类型的索引
                    for (int k = 0; k < 3; ++k) {
                        size_t offset = (i + k);
                        if (indAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            idx[k] = *reinterpret_cast<const uint16_t *>(
                                dataPtr + offset * (indStride ? indStride : 2));
                        } else if (indAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                            idx[k] = *reinterpret_cast<const uint32_t *>(
                                dataPtr + offset * (indStride ? indStride : 4));
                        } else if (indAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            idx[k] = *reinterpret_cast<const uint8_t *>(
                                dataPtr + offset * (indStride ? indStride : 1));
                        } else {
                            idx[k] = 0;
                        }
                    }

                    // 【关键】加上 globalVertOffset
                    m.face[globalFaceOffset].V(0) = &m.vert[globalVertOffset + idx[0]];
                    m.face[globalFaceOffset].V(1) = &m.vert[globalVertOffset + idx[1]];
                    m.face[globalFaceOffset].V(2) = &m.vert[globalVertOffset + idx[2]];

                    // Set Material ID
                    m.face[globalFaceOffset].matId = primitive.material;

                    if (texCoords) {
                        m.face[globalFaceOffset].WT(0) = MyFace::TexCoordType(
                            texCoords[idx[0] * texStride], texCoords[idx[0] * texStride + 1]);
                        m.face[globalFaceOffset].WT(1) = MyFace::TexCoordType(
                            texCoords[idx[1] * texStride], texCoords[idx[1] * texStride + 1]);
                        m.face[globalFaceOffset].WT(2) = MyFace::TexCoordType(
                            texCoords[idx[2] * texStride], texCoords[idx[2] * texStride + 1]);
                    }
                    globalFaceOffset++;
                }
            }

            // 更新全局偏移
            globalVertOffset += currentPrimVertCount;

            printf("  Processing Primitive %d (Material Index: %d, Indices: %d) Done.\n", primIdx++,
                   primitive.material, primitive.indices);
        }
    }
    printf("LoadGLB: Finished. Loaded %d vertices, %d faces.\n", globalVertOffset,
           globalFaceOffset);
    return true;
}

// --- 5. GLB 保存器 (已修复编译错误与MeshLab兼容性) ---
bool SaveGLB(MyMesh &m, const tinygltf::Model &originalModel, const std::string &filename) {
    tinygltf::Model outModel;

    // 1. 复制配置
    outModel.asset           = originalModel.asset;
    outModel.asset.generator = "VCG-Simplifier";

    // 【关键修复】处理图片和纹理引用
    outModel.materials = originalModel.materials;
    outModel.textures  = originalModel.textures;
    outModel.samplers  = originalModel.samplers;
    outModel.images    = originalModel.images;

    // 清除 BufferView 索引，强制 TinyGLTF 重新打包图片数据
    for (auto &img : outModel.images) {
        img.bufferView = -1;
        img.uri        = "";
    }

    // 2. 准备几何数据容器
    std::vector<float> rawPos;
    std::vector<float> rawNor;
    std::vector<float> rawUV;
    std::vector<unsigned char> rawColorUB;

    // Split indices by material ID
    std::map<int, std::vector<unsigned int>> materialGroups;

    // 用于裂解顶点 (Wedge UV -> Vertex Attributes)
    struct VertKey {
        int vIdx;
        float u, v;
        bool operator<(const VertKey &o) const {
            if (vIdx != o.vIdx)
                return vIdx < o.vIdx;
            if (u != o.u)
                return u < o.u;
            return v < o.v;
        }
    };
    std::map<VertKey, unsigned int> uniqueMap;
    int idxCounter = 0;

    // 【关键修复】计算包围盒 (Min/Max)，MeshLab 必须需要这个
    std::vector<double> posMin = {1e9, 1e9, 1e9};
    std::vector<double> posMax = {-1e9, -1e9, -1e9};

    for (auto &f : m.face) {
        if (f.IsD())
            continue;
        for (int i = 0; i < 3; ++i) {
            int vIdx = vcg::tri::Index(m, f.V(i));
            float u  = f.WT(i).u();
            float v  = f.WT(i).v();

            VertKey key = {vIdx, u, v};
            auto it     = uniqueMap.find(key);

            unsigned int finalIdx;
            if (it != uniqueMap.end()) {
                finalIdx = it->second;
            } else {
                uniqueMap[key] = idxCounter;
                finalIdx       = idxCounter;

                float px = f.V(i)->P().X();
                float py = f.V(i)->P().Y();
                float pz = f.V(i)->P().Z();

                rawPos.push_back(px);
                rawPos.push_back(py);
                rawPos.push_back(pz);

                // 更新 Min/Max
                if (px < posMin[0])
                    posMin[0] = px;
                if (px > posMax[0])
                    posMax[0] = px;
                if (py < posMin[1])
                    posMin[1] = py;
                if (py > posMax[1])
                    posMax[1] = py;
                if (pz < posMin[2])
                    posMin[2] = pz;
                if (pz > posMax[2])
                    posMax[2] = pz;

                rawNor.push_back(f.V(i)->N().X());
                rawNor.push_back(f.V(i)->N().Y());
                rawNor.push_back(f.V(i)->N().Z());

                rawUV.push_back(u);
                rawUV.push_back(v);

                // Color
                vcg::Color4b c = f.V(i)->C();
                rawColorUB.push_back(c[0]);
                rawColorUB.push_back(c[1]);
                rawColorUB.push_back(c[2]);
                rawColorUB.push_back(c[3]);

                idxCounter++;
            }
            // Store index in the appropriate material group
            materialGroups[f.matId].push_back(finalIdx);
        }
    }

    // Flatten indices for buffer creation
    std::vector<unsigned int> allIndices;
    // Store offsets to create accessors later
    struct MatGroupInfo {
        int matId;
        size_t indexStart;
        size_t indexCount;
    };
    std::vector<MatGroupInfo> matGroupInfos;

    for (auto &pair : materialGroups) {
        MatGroupInfo info;
        info.matId      = pair.first;
        info.indexStart = allIndices.size(); // Offset in count, not bytes
        info.indexCount = pair.second.size();
        matGroupInfos.push_back(info);

        allIndices.insert(allIndices.end(), pair.second.begin(), pair.second.end());
    }

    // 3. 构建二进制 Buffer
    tinygltf::Buffer buffer;

    size_t lenPos = rawPos.size() * sizeof(float);
    size_t lenNor = rawNor.size() * sizeof(float);
    size_t lenUV  = rawUV.size() * sizeof(float);
    size_t lenCol = rawColorUB.size() * sizeof(unsigned char);
    size_t lenInd = allIndices.size() * sizeof(unsigned int);

    size_t offsetPos = 0;
    size_t offsetNor = offsetPos + lenPos;
    size_t offsetUV  = offsetNor + lenNor;
    size_t offsetCol = offsetUV + lenUV;
    size_t offsetInd = offsetCol + lenCol;

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
    std::memcpy(buffer.data.data() + offsetUV, rawUV.data(), lenUV);
    std::memcpy(buffer.data.data() + offsetCol, rawColorUB.data(), lenCol);
    std::memcpy(buffer.data.data() + offsetInd, allIndices.data(), lenInd);

    outModel.buffers.push_back(buffer);
    int bufferId = 0;

    // 4. 创建 BufferViews
    auto addBufferView = [&](size_t offset, size_t length, int target) {
        tinygltf::BufferView bv;
        bv.buffer     = bufferId;
        bv.byteOffset = offset;
        bv.byteLength = length;
        bv.target     = target;
        outModel.bufferViews.push_back(bv);
        return (int)outModel.bufferViews.size() - 1;
    };

    int bvPos = addBufferView(offsetPos, lenPos, TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvNor = addBufferView(offsetNor, lenNor, TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvUV  = addBufferView(offsetUV, lenUV, TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvCol = addBufferView(offsetCol, lenCol, TINYGLTF_TARGET_ARRAY_BUFFER);
    int bvInd = addBufferView(offsetInd, lenInd, TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

    // 5. 创建 Accessors
    // 【关键修复】参数 typeEnum 改为 int，并接收 Min/Max
    auto addAccessor = [&](int bv, int count, int compType, int typeEnum,
                           const std::vector<double> *minV, const std::vector<double> *maxV,
                           bool normalized = false, size_t byteOffset = 0) {
        tinygltf::Accessor acc;
        acc.bufferView    = bv;
        acc.byteOffset    = byteOffset;
        acc.componentType = compType;
        acc.count         = count;
        acc.type          = typeEnum; // 这里使用 TINYGLTF_TYPE_*
        acc.normalized    = normalized;
        if (minV)
            acc.minValues = *minV;
        if (maxV)
            acc.maxValues = *maxV;
        outModel.accessors.push_back(acc);
        return (int)outModel.accessors.size() - 1;
    };

    // Position 必须要有 Min/Max
    int accPos = addAccessor(bvPos, (int)rawPos.size() / 3, TINYGLTF_COMPONENT_TYPE_FLOAT,
                             TINYGLTF_TYPE_VEC3, &posMin, &posMax);
    int accNor = addAccessor(bvNor, (int)rawNor.size() / 3, TINYGLTF_COMPONENT_TYPE_FLOAT,
                             TINYGLTF_TYPE_VEC3, nullptr, nullptr);
    int accUV  = addAccessor(bvUV, (int)rawUV.size() / 2, TINYGLTF_COMPONENT_TYPE_FLOAT,
                             TINYGLTF_TYPE_VEC2, nullptr, nullptr);
    // Color: VEC4 Unsigned Byte Normalized
    int accCol =
        addAccessor(bvCol, (int)rawColorUB.size() / 4, TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                    TINYGLTF_TYPE_VEC4, nullptr, nullptr, true);

    // 6. 组装 Mesh
    tinygltf::Mesh mesh;

    // Create primitives for each material group
    printf("SaveGLB: Found %d unique vertices.\n", idxCounter);
    printf("SaveGLB: Splitting into %d primitives based on material IDs.\n",
           (int)matGroupInfos.size());

    for (const auto &info : matGroupInfos) {
        printf("  MatGroup: MatID %d, IndexCount %d\n", info.matId, (int)info.indexCount);

        // Choose easiest index type based on vertex count
        int indexCompType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT; // Default to 4 bytes
        if (idxCounter < 65536) {
            indexCompType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT; // Optimize to 2 bytes
        }

        // Create an accessor for this chunk of indices
        // IMPORTANT: We need to write the data to the buffer in the correct format!
        // The current 'allIndices' vector is std::vector<unsigned int> (4 bytes).
        // If we want 2-byte indices, we need to convert and write a separate buffer view or handle
        // conversion.

        // For simplicity in this step, let's keep it robust first and just check the size logic
        // later. Or better, let's stick to UINT for now to ensure correctness, and just log the
        // overhead.

        size_t accByteOffset = info.indexStart * sizeof(unsigned int);
        int accInd = addAccessor(bvInd, (int)info.indexCount, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT,
                                 TINYGLTF_TYPE_SCALAR, nullptr, nullptr, false, accByteOffset);

        tinygltf::Primitive prim;
        prim.attributes["POSITION"]   = accPos;
        prim.attributes["NORMAL"]     = accNor;
        prim.attributes["TEXCOORD_0"] = accUV;
        prim.attributes["COLOR_0"]    = accCol;
        prim.indices                  = accInd;
        prim.material                 = info.matId; // Set Material ID
        prim.mode                     = TINYGLTF_MODE_TRIANGLES;

        mesh.primitives.push_back(prim);
    }

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
