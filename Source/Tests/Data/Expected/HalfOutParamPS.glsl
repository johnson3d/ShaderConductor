#version 300
#if defined(GL_AMD_gpu_shader_half_float)
#extension GL_AMD_gpu_shader_half_float : require
#elif defined(GL_NV_gpu_shader5)
#extension GL_NV_gpu_shader5 : require
#else
#error No extension available for FP16.
#endif
#extension GL_ARB_separate_shader_objects : require

out vec4 out_var_SV_Target0;

void main()
{
    out_var_SV_Target0 = vec4(vec3(cross(f16vec3(float16_t(1.0), float16_t(0.0), float16_t(0.0)), f16vec3(float16_t(0.0), float16_t(1.0), float16_t(0.0)))), 1.0);
}

