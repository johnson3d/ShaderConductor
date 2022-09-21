#version 300
#extension GL_ARB_separate_shader_objects : require

uniform sampler2D subpassInput0;
uniform usampler2D subpassInput1;
uniform sampler2DMS subpassInput2;

out vec4 out_var_SV_Target0;
out uint out_var_SV_Target1;
out vec2 out_var_SV_Target2;

void main()
{
    out_var_SV_Target0 = (texelFetch(subpassInput0, ivec2(gl_FragCoord.xy), 0) + vec4(float(texelFetch(subpassInput1, ivec2(gl_FragCoord.xy), 0).x))) + vec4(texelFetch(subpassInput2, ivec2(gl_FragCoord.xy), 1).xy, 0.0, 0.0);
    out_var_SV_Target1 = 0u;
    out_var_SV_Target2 = vec2(0.0);
}

