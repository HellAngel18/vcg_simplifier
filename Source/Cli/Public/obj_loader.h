#pragma once
#include "mymesh.h"

bool LoadObj(MyMesh &m, const std::string &filename);
bool SaveObj(MyMesh &m, const std::string &filename);