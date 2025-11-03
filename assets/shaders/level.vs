#version 330 core


layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_UV;
layout (location = 2) in vec3 a_normal;


out VS_OUT {
    vec2 UV;
    vec3 normal;
    vec4 frag_pos_world;
} vs_out;


/* Transform uniforms */
uniform mat4 u_model_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;


void main()
{
    vs_out.UV = a_UV;
    vs_out.normal = a_normal;
    
    vs_out.frag_pos_world = u_model_mat * vec4(a_pos, 1.0);

    gl_Position = u_proj_mat * u_view_mat * vs_out.frag_pos_world;

    // Vertex snapping
    float distance_from_cam = clamp(gl_Position.w, -0.1, 1000.0);
    float pos_resolution = 640.0 * 0.2;
    gl_Position.xy = round(gl_Position.xy * (pos_resolution / distance_from_cam)) / (pos_resolution / distance_from_cam);

    // Affine mapping
    gl_Position /= mix(abs(gl_Position.w), 1.0, 0.5);
}
