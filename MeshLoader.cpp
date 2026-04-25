#define NOMINMAX
#include "MeshLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

namespace gfw {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
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
    return std::ifstream(filename, std::ios::binary);
}

static int ParseObjInt(const std::string &s) {
    if (s.empty()) return 0;
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}

struct ParsedMtl {
    std::wstring diffuse_texture;
    DirectX::XMFLOAT4 kd = {1.0f, 1.0f, 1.0f, 1.0f};
};

static std::wstring DirectoryOf(const std::wstring &path) {
    size_t slash = path.find_last_of(L"/\\");
    if (slash == std::wstring::npos) return L".";
    return path.substr(0, slash);
}

static std::wstring JoinPath(const std::wstring &base, const std::wstring &relative) {
    if (relative.empty()) return base;
    if (relative.size() > 1 && relative[1] == L':') return relative;
    if (!relative.empty() && (relative[0] == L'/' || relative[0] == L'\\')) return relative;
    if (base.empty()) return relative;
    wchar_t last = base.back();
    if (last == L'/' || last == L'\\') return base + relative;
    return base + L"/" + relative;
}

static std::unordered_map<std::wstring, ParsedMtl> ParseMtl(
        const std::wstring &obj_dir,
        const std::wstring &mtl_filename) {
    std::unordered_map<std::wstring, ParsedMtl> result;
    if (mtl_filename.empty()) {
        return result;
    }
    std::wstring mtl_path = JoinPath(obj_dir, mtl_filename);
    std::ifstream file(mtl_path, std::ios::binary);
    if (!file.is_open()) {
        // Support callers that already pass a full/usable path to MTL.
        mtl_path = mtl_filename;
        file = std::ifstream(mtl_path, std::ios::binary);
        if (!file.is_open()) {
            return result;
        }
    }

    std::wstring current;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string tok;
        ss >> tok;
        if (tok == "newmtl") {
            std::string name;
            ss >> name;
            current.assign(name.begin(), name.end());
            result[current] = ParsedMtl{};
        } else if (tok == "map_Kd" && !current.empty()) {
            std::string tex;
            ss >> tex;
            std::wstring wtex(tex.begin(), tex.end());
            result[current].diffuse_texture = JoinPath(DirectoryOf(mtl_path), wtex);
        } else if (tok == "Kd" && !current.empty()) {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (ss >> r >> g >> b) {
                result[current].kd = {r, g, b, 1.0f};
            }
        }
    }
    return result;
}

ObjModelData MeshLoader::LoadObjModel(const std::wstring &obj_filename, const std::wstring &mtl_filename) {
    ObjModelData model;
    std::ifstream file = OpenObjStream(obj_filename);
    if (!file.is_open()) {
        std::wcerr << L"Failed to open OBJ: " << obj_filename << std::endl;
        return model;
    }

    const auto mtls = ParseMtl(DirectoryOf(obj_filename), mtl_filename);

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> uvs;
    const float scale = 0.01f;

    std::unordered_map<std::wstring, size_t> material_to_submesh;
    std::vector<std::vector<Vertex>> vertices_per_submesh;
    std::vector<std::vector<std::uint32_t>> indices_per_submesh;
    std::vector<std::map<ObjIndex, uint32_t>> index_map_per_submesh;

    auto ensure_submesh = [&](const std::wstring &material_name) -> size_t {
        auto it = material_to_submesh.find(material_name);
        if (it != material_to_submesh.end()) {
            return it->second;
        }
        size_t idx = model.submeshes.size();
        material_to_submesh[material_name] = idx;
        model.submeshes.push_back({});
        model.submeshes[idx].material_name = material_name;
        auto mtl_it = mtls.find(material_name);
        if (mtl_it != mtls.end()) {
            model.submeshes[idx].diffuse_texture_path = mtl_it->second.diffuse_texture;
            model.submeshes[idx].albedo = mtl_it->second.kd;
        }
        vertices_per_submesh.emplace_back();
        indices_per_submesh.emplace_back();
        index_map_per_submesh.emplace_back();
        return idx;
    };

    size_t current_submesh = ensure_submesh(L"default");

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
        } else if (type == "vt") {
            XMFLOAT2 uv = {};
            if (!(ss >> uv.x >> uv.y)) continue;
            uv.y = 1.0f - uv.y;
            uvs.push_back(uv);
        } else if (type == "vn") {
            XMFLOAT3 n;
            if (!(ss >> n.x >> n.y >> n.z)) continue;
            n.z = -n.z;
            normals.push_back(n);
        } else if (type == "usemtl") {
            std::string mat;
            ss >> mat;
            std::wstring wmat(mat.begin(), mat.end());
            current_submesh = ensure_submesh(wmat);
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
                    idx.vt = ParseObjInt(vertexStr.substr(firstSlash + 1));
                } else {
                    idx.v = ParseObjInt(vertexStr.substr(0, firstSlash));
                    idx.vt = ParseObjInt(vertexStr.substr(firstSlash + 1, secondSlash - firstSlash - 1));
                    idx.vn = ParseObjInt(vertexStr.substr(secondSlash + 1));
                }
                faceIndices.push_back(idx);
            }

            const int posCount = static_cast<int>(positions.size());
            const int normCount = static_cast<int>(normals.size());
            const int uvCount = static_cast<int>(uvs.size());
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                ObjIndex tri[3] = { faceIndices[0], faceIndices[i + 1], faceIndices[i] };
                for (int k = 0; k < 3; ++k) {
                    ObjIndex &objIdx = tri[k];
                    if (objIdx.v < 0) objIdx.v = posCount + objIdx.v + 1;
                    if (objIdx.vt < 0) objIdx.vt = uvCount + objIdx.vt + 1;
                    if (objIdx.vn < 0) objIdx.vn = normCount + objIdx.vn + 1;
                    objIdx.v = std::max(1, std::min(objIdx.v, posCount));
                    objIdx.vt = std::max(0, std::min(objIdx.vt, uvCount));
                    objIdx.vn = std::max(0, std::min(objIdx.vn, normCount));

                    auto &sub_vertices = vertices_per_submesh[current_submesh];
                    auto &sub_indices = indices_per_submesh[current_submesh];
                    auto &sub_index_map = index_map_per_submesh[current_submesh];

                    if (sub_index_map.find(objIdx) == sub_index_map.end()) {
                        Vertex v = {};
                        v.px = positions[objIdx.v - 1].x;
                        v.py = positions[objIdx.v - 1].y;
                        v.pz = positions[objIdx.v - 1].z;
                        if (objIdx.vt > 0) {
                            v.u = uvs[objIdx.vt - 1].x;
                            v.v = uvs[objIdx.vt - 1].y;
                        }
                        if (objIdx.vn > 0) {
                            v.nx = normals[objIdx.vn - 1].x;
                            v.ny = normals[objIdx.vn - 1].y;
                            v.nz = normals[objIdx.vn - 1].z;
                        }
                        uint32_t newIdx = static_cast<uint32_t>(sub_vertices.size());
                        sub_vertices.push_back(v);
                        sub_index_map[objIdx] = newIdx;
                    }
                    sub_indices.push_back(sub_index_map[objIdx]);
                }
            }
        }
    }

    for (size_t i = 0; i < model.submeshes.size(); ++i) {
        auto &sub = model.submeshes[i];
        auto &sub_vertices = vertices_per_submesh[i];
        auto &sub_indices = indices_per_submesh[i];
        sub.mesh.vertex_stride = sizeof(Vertex);
        sub.mesh.vertex_count = static_cast<uint32_t>(sub_vertices.size());
        sub.mesh.indices = std::move(sub_indices);
        if (sub.mesh.vertex_count > 0) {
            sub.mesh.vertex_data.resize(sub.mesh.vertex_count * sub.mesh.vertex_stride);
            std::memcpy(sub.mesh.vertex_data.data(), sub_vertices.data(), sub.mesh.vertex_data.size());
        }
    }
    return model;
}

MeshData MeshLoader::LoadObj(const std::wstring &filename) {
    ObjModelData model = LoadObjModel(filename);
    for (const auto &sub : model.submeshes) {
        if (sub.mesh.vertex_count > 0) {
            return sub.mesh;
        }
    }
    return {};
}
}
