#version 330 core


#define TRIPLANAR_BLEND_SHARPNESS 1.0


in VS_OUT {
    vec2 UV;
    vec3 normal;
    vec4 frag_pos_world;
} vs_out;


out vec4 fragColor;


uniform sampler2D u_texture;


vec4 sample_triplanar(sampler2D texture_sampler, vec3 triplanar_power_normal, vec3 triplanar_pos);


void main()
{
    vec3 triplanar_power_normal = pow(abs(vs_out.normal), vec3(TRIPLANAR_BLEND_SHARPNESS));
    triplanar_power_normal /= dot(triplanar_power_normal, vec3(1.0));

    fragColor = sample_triplanar(u_texture, triplanar_power_normal, vs_out.frag_pos_world.xyz * 0.5);
}


vec4 sample_triplanar(sampler2D texture_sampler, vec3 triplanar_power_normal, vec3 triplanar_pos)
{
    vec4 sample;
    sample += texture(texture_sampler, triplanar_pos.xy) * triplanar_power_normal.z;
    sample += texture(texture_sampler, triplanar_pos.xz) * triplanar_power_normal.y;
    sample += texture(texture_sampler, triplanar_pos.zy) * triplanar_power_normal.x;
    return sample;
}
