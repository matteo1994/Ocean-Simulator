#version 430 core
layout (location = 0) in vec3 aPos;

out vec3 localPos;

uniform mat4 Proj;
uniform mat4 View;
uniform mat4 MVP;

void main()
{
    localPos = aPos;  
    gl_Position =  Proj * View * vec4(localPos, 1.0);
}


