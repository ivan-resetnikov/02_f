#version 330 core


layout (location = 0) in vec3 a_pos;


// out VS_OUT {
// } vs_out;


/* Transform uniforms */
uniform mat4 u_model_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;


void main()
{
    gl_Position = u_proj_mat * u_view_mat * u_model_mat * vec4(a_pos, 1.0);

    // Vertex snapping
    // float distance_from_cam = clamp(gl_Position.w, -0.1, 1000.0);
    // float pos_resolution = 640.0 * 0.2;
    // gl_Position.xy = round(gl_Position.xy * (pos_resolution / distance_from_cam)) / (pos_resolution / distance_from_cam);

    // Affine mapping
    // gl_Position /= mix(abs(gl_Position.w), 1.0, 0.5);
}
