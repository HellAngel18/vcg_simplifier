#pragma once
#include "mymesh.h"
#include <tiny_gltf.h>

bool LoadGLB(MyMesh &m, tinygltf::Model &outModel, const std::string &filename);
bool SaveGLB(MyMesh &m, const tinygltf::Model &originalModel, const std::string &filename);