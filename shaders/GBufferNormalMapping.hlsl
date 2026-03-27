// Tangent-space normal map → world space (cotangent frame, no vertex tangents required).
// Most downloaded normal maps are OpenGL-style; flip green for common DirectX conventions.

#ifndef GFW_NORMALMAP_OPENGL_Y
#define GFW_NORMALMAP_OPENGL_Y 1
#endif

float3 DecodeNormalMapSample(float4 samp)
{
    float3 n = samp.rgb * 2.0 - 1.0;
#if GFW_NORMALMAP_OPENGL_Y
    n.y = -n.y;
#endif
    return n;
}

float3 NormalFromTsToWorld(float3 Nw_vertex, float3 posW, float2 uv, float3 nTS)
{
    float3 dpdx = ddx(posW);
    float3 dpdy = ddy(posW);

    // Macro-orientation of the actual (displaced) surface in the pixel quad — required when
    // tessellation/height moves geometry but vertex normals stay smooth (flat wall + bumps).
    float3 geomN = normalize(cross(dpdx, dpdy));
    if (dot(geomN, Nw_vertex) < 0.0)
    {
        geomN = -geomN;
    }

    float2 duvx = ddx(uv);
    float2 duvy = ddy(uv);

    float det = duvx.x * duvy.y - duvx.y * duvy.x;
    if (abs(det) < 1e-8)
    {
        return geomN;
    }

    float3 Tw = (dpdx * duvy.y - dpdy * duvx.y) / det;
    Tw = normalize(Tw - dot(geomN, Tw) * geomN);
    float3 Bw = cross(geomN, Tw);
    return normalize(nTS.x * Tw + nTS.y * Bw + nTS.z * geomN);
}

void BuildTriangleTBN(
    float3 p0, float3 p1, float3 p2,
    float2 uv0, float2 uv1, float2 uv2,
    float3 N,
    out float3 T,
    out float3 B)
{
    float3 Q1 = p1 - p0;
    float3 Q2 = p2 - p0;
    float2 duv1 = uv1 - uv0;
    float2 duv2 = uv2 - uv0;
    float det = duv1.x * duv2.y - duv2.x * duv1.y;

    if (abs(det) < 1e-8)
    {
        T = float3(1.0, 0.0, 0.0);
        T = normalize(T - dot(N, T) * N);
        B = cross(N, T);
        return;
    }

    T = (Q1 * duv2.y - Q2 * duv1.y) / det;
    T = normalize(T - dot(N, T) * N);
    B = cross(N, T);
}

float3 NormalFromTsWithTBN(float3 nTS, float3 N, float3 T, float3 B)
{
    return normalize(nTS.x * T + nTS.y * B + nTS.z * N);
}
