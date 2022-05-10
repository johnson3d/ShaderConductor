#version 300
#if defined(GL_AMD_gpu_shader_half_float)
#extension GL_AMD_gpu_shader_half_float : require
#elif defined(GL_NV_gpu_shader5)
#extension GL_NV_gpu_shader5 : require
#else
#error No extension available for FP16.
#endif
#extension GL_ARB_separate_shader_objects : require

layout(std430) readonly buffer type_StructuredBuffer_half
{
    float16_t _m0[];
} _buffer;

layout(location = 0) flat in uint varying_TEXCOORD0;
out vec4 out_var_SV_Target0;

void main()
{
    out_var_SV_Target0 = vec4(float(_buffer._m0[varying_TEXCOORD0]), 0.0, 0.0, 1.0);
}

