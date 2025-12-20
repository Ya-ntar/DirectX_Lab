#include "MeshLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <map>
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
        
        bool operator<(const ObjIndex& other) const {
             if (v != other.v) return v < other.v;
             if (vt != other.vt) return vt < other.vt;
             return vn < other.vn;
        }
    };

    MeshData MeshLoader::LoadObj(const std::wstring& filename) {
        MeshData meshData;
        
        // Convert wstring to string for ifstream
        std::string filenameStr(filename.begin(), filename.end());
        std::ifstream file(filenameStr);
        if (!file.is_open()) {
            std::wcerr << L"Failed to open OBJ file: " << filename << std::endl;
            return meshData;
        }

        std::vector<XMFLOAT3> positions;
        std::vector<XMFLOAT3> normals;
        
        std::vector<Vertex> finalVertices;
        std::map<ObjIndex, uint32_t> indexMap;

        // Scale factor to make Sponza manageable if it's too big
        // Usually Sponza is ~2000 units. Unit cube is 2 units.
        // Let's scale by 0.01f
        const float scale = 0.01f;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v") {
                XMFLOAT3 p;
                ss >> p.x >> p.y >> p.z;
                p.x *= scale;
                p.y *= scale;
                p.z *= scale;
                
                // Convert to Left-Handed: Flip Z
                p.z = -p.z;
                positions.push_back(p);
            } else if (type == "vn") {
                XMFLOAT3 n;
                ss >> n.x >> n.y >> n.z;
                // Flip Z for normals
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
                        idx.v = std::stoi(vertexStr);
                    } else if (secondSlash == std::string::npos) {
                        idx.v = std::stoi(vertexStr.substr(0, firstSlash));
                    } else {
                        idx.v = std::stoi(vertexStr.substr(0, firstSlash));
                        idx.vn = std::stoi(vertexStr.substr(secondSlash + 1));
                    }
                    faceIndices.push_back(idx);
                }

                // Triangulate fan
                // Because we flipped Z, we need to reverse winding order to maintain Front Face
                // Standard OBJ is CCW. Flipping Z makes it CW (if looking from same direction).
                // But we want CCW in our LH system?
                // DirectX default is Backface culling (CullMode = D3D12_CULL_MODE_BACK).
                // Front face is usually CW or CCW depending on settings.
                // D3D12 default is CCW is Front.
                // If we flip Z, a CCW triangle (v0, v1, v2) becomes CW in 3D space relative to camera?
                // Actually, (x, y, z) -> (x, y, -z) is a reflection. Reflection inverts orientation.
                // So CCW becomes CW.
                // So we need to swap vertices to make it CCW again.
                // So instead of (0, i, i+1), use (0, i+1, i).
                
                for (size_t i = 1; i < faceIndices.size() - 1; ++i) {
                    ObjIndex indices[3] = { faceIndices[0], faceIndices[i+1], faceIndices[i] };
                    
                    for (int k = 0; k < 3; ++k) {
                        ObjIndex& objIdx = indices[k];
                        
                        // Handle negative indices
                        if (objIdx.v < 0) objIdx.v = static_cast<int>(positions.size()) + objIdx.v + 1;
                        if (objIdx.vn < 0) objIdx.vn = static_cast<int>(normals.size()) + objIdx.vn + 1;

                        if (indexMap.find(objIdx) == indexMap.end()) {
                            Vertex v = {};
                            if (objIdx.v > 0 && objIdx.v <= static_cast<int>(positions.size())) {
                                v.px = positions[objIdx.v - 1].x;
                                v.py = positions[objIdx.v - 1].y;
                                v.pz = positions[objIdx.v - 1].z;
                            }
                            if (objIdx.vn > 0 && objIdx.vn <= static_cast<int>(normals.size())) {
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

        std::cout << "Loaded OBJ: " << filenameStr << " Vertices: " << meshData.vertex_count << " Indices: " << meshData.indices.size() << std::endl;

        return meshData;
    }
}
