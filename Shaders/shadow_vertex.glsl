#version 330 core
layout (location = 0) in vec3 position;

uniform mat4 transform_in_light_space;

void main()
{

    gl_Position = transform_in_light_space * vec4(position, 1.0);
}
