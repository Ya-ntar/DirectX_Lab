#include "TessellationCommon.hlsl"

HSConstantData HSConst(InputPatch<VSOutput, 4> patch) {
    HSConstantData const_data;

    float tess = clamp((tessParams.x + tessParams.y) * 0.5f, tessParams.x, tessParams.y);
    tess = max(tess, 1.0f);

    const_data.edges[0] = tess;
    const_data.edges[1] = tess;
    const_data.edges[2] = tess;
    const_data.edges[3] = tess;
    const_data.inside[0] = tess;
    const_data.inside[1] = tess;
    return const_data;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConst")]
VSOutput HSMain(InputPatch<VSOutput, 4> patch, uint cp_id : SV_OutputControlPointID) {
    return patch[cp_id];
}
