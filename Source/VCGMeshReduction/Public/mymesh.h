#pragma once
// VCG Headers
#include <vcg/complex/complex.h>

// Algorithms
#include <vcg/complex/algorithms/edge_collapse.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric_tex.h>

// 引入 SimpleTempData 用于管理临时数据 [MeshLab 关键依赖]
#include <vcg/container/simple_temporary_data.h>

// --- 2. 网格定义 ---
class MyVertex;
class MyEdge;
class MyFace;
struct MyUsedTypes
    : public vcg::UsedTypes<vcg::Use<MyVertex>::AsVertexType, vcg::Use<MyEdge>::AsEdgeType,
                            vcg::Use<MyFace>::AsFaceType> {};
class MyVertex : public vcg::Vertex<MyUsedTypes, vcg::vertex::Coord3f, vcg::vertex::Normal3f,
                                    vcg::vertex::Color4b, vcg::vertex::BitFlags, vcg::vertex::Mark,
                                    vcg::vertex::VFAdj> {};
class MyFace : public vcg::Face<MyUsedTypes, vcg::face::VertexRef, vcg::face::Normal3f,
                                vcg::face::WedgeTexCoord2f, vcg::face::BitFlags, vcg::face::Mark,
                                vcg::face::FFAdj, vcg::face::VFAdj> {
  public:
    int matId = 0;
};
class MyEdge : public vcg::Edge<MyUsedTypes> {};
class MyMesh
    : public vcg::tri::TriMesh<std::vector<MyVertex>, std::vector<MyFace>, std::vector<MyEdge>> {};

// --- 3. 简化类定义 ---
typedef vcg::tri::BasicVertexPair<MyVertex> MyVertexPair;
class MyCollapse : public vcg::tri::TriEdgeCollapseQuadricTex<MyMesh, MyVertexPair, MyCollapse,
                                                              vcg::tri::QuadricTexHelper<MyMesh>> {
  public:
    using vcg::tri::TriEdgeCollapseQuadricTex<
        MyMesh, MyVertexPair, MyCollapse,
        vcg::tri::QuadricTexHelper<MyMesh>>::TriEdgeCollapseQuadricTex;
};