#version 330 core


// in VS_OUT {
// } vs_out;


out vec4 fragColor;


void main()
{
    fragColor = vec4(1.0);
    // fragColor = texture(u_texture, vs_out.UV);
}
