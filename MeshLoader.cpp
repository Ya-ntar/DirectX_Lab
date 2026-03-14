#define NOMINMAX
#include "MeshLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <DirectXMath.h>

using namespace DirectX;

namespace gfw {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

struct ObjIndex {
    int v = 0;
    int vt = 0;
    int vn = 0;
    bool operator<(const ObjIndex &other) const {
        if (v != other.v) return v < other.v;
        if (vt != other.vt) return vt < other.vt;
        return vn < other.vn;
    }
};

static std::ifstream OpenObjStream(const std::wstring &filename) {
    return std::ifstream(std::filesystem::path(filename), std::ios::binary);
}

static int ParseObjInt(const std::string &s) {
    if (s.empty()) return 0;
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}

MeshData MeshLoader::LoadObj(const std::wstring &filename) {
    MeshData meshData;
    std::ifstream file = OpenObjStream(filename);
    if (!file.is_open()) {
        std::wcerr << L"Failed to open OBJ: " << filename << std::endl;
        return meshData;
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<Vertex> finalVertices;
    std::map<ObjIndex, uint32_t> indexMap;
    const float scale = 0.01f;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            XMFLOAT3 p;
            if (!(ss >> p.x >> p.y >> p.z)) continue;
            p.x *= scale; p.y *= scale; p.z *= scale;
            p.z = -p.z;
            positions.push_back(p);
        } else if (type == "vn") {
            XMFLOAT3 n;
            if (!(ss >> n.x >> n.y >> n.z)) continue;
            n.z = -n.z;
            normals.push_back(n);
        } else if (type == "f") {
            std::string vertexStr;
            std::vector<ObjIndex> faceIndices;
            while (ss >> vertexStr) {
                ObjIndex idx;
                size_t firstSlash = vertexStr.find('/');
                size_t secondSlash = vertexStr.find('/', firstSlash + 1);
                if (firstSlash == std::string::npos) {
                    idx.v = ParseObjInt(vertexStr);
                } else if (secondSlash == std::string::npos) {
                    idx.v = ParseObjInt(vertexStr.substr(0, firstSlash));
                } else {
                    idx.v = ParseObjInt(vertexStr.substr(0, firstSlash));
                    idx.vn = ParseObjInt(vertexStr.substr(secondSlash + 1));
                }
                faceIndices.push_back(idx);
            }

            const int posCount = static_cast<int>(positions.size());
            const int normCount = static_cast<int>(normals.size());
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                ObjIndex tri[3] = { faceIndices[0], faceIndices[i + 1], faceIndices[i] };
                for (int k = 0; k < 3; ++k) {
                    ObjIndex &objIdx = tri[k];
                    if (objIdx.v < 0) objIdx.v = posCount + objIdx.v + 1;
                    if (objIdx.vn < 0) objIdx.vn = normCount + objIdx.vn + 1;
                    objIdx.v = std::max(1, std::min(objIdx.v, posCount));
                    objIdx.vn = std::max(0, std::min(objIdx.vn, normCount));

                    if (indexMap.find(objIdx) == indexMap.end()) {
                        Vertex v = {};
                        v.px = positions[objIdx.v - 1].x;
                        v.py = positions[objIdx.v - 1].y;
                        v.pz = positions[objIdx.v - 1].z;
                        if (objIdx.vn > 0) {
                            v.nx = normals[objIdx.vn - 1].x;
                            v.ny = normals[objIdx.vn - 1].y;
                            v.nz = normals[objIdx.vn - 1].z;
                        }
                        uint32_t newIdx = static_cast<uint32_t>(finalVertices.size());
                        finalVertices.push_back(v);
                        indexMap[objIdx] = newIdx;
                    }
                    meshData.indices.push_back(indexMap[objIdx]);
                }
            }
        }
    }

    meshData.vertex_stride = sizeof(Vertex);
    meshData.vertex_count = static_cast<uint32_t>(finalVertices.size());
    if (meshData.vertex_count > 0) {
        meshData.vertex_data.resize(meshData.vertex_count * meshData.vertex_stride);
        std::memcpy(meshData.vertex_data.data(), finalVertices.data(), meshData.vertex_data.size());
    }
    return meshData;
}
}
