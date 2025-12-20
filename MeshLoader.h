#pragma once

#include "MeshData.h"
#include <string>

namespace gfw {
    class MeshLoader {
    public:
        static MeshData LoadObj(const std::wstring& filename);
    };
}
