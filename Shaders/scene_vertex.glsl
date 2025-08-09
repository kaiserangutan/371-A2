#version 330 core
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;     

uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform mat4 light_proj_view_matrix;

out vec3 fragment_normal;
out vec3 fragment_position;
out vec4 fragment_position_light_space;
out vec2 vUV;                            

void main()
{
    vec4 worldPos = model_matrix * vec4(in_position, 1.0);
    fragment_position = worldPos.xyz;

    // normal: use normal matrix
    mat3 normalMatrix = mat3(transpose(inverse(model_matrix)));
    fragment_normal = normalize(normalMatrix * in_normal);

    fragment_position_light_space = light_proj_view_matrix * worldPos;

    vUV = in_uv;

    gl_Position = projection_matrix * view_matrix * worldPos;
}
