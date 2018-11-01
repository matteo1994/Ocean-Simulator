
#version 430 core


layout (location = 0) in vec3 aPos;

uniform mat4 Proj;
uniform mat4 View;
uniform mat4 model;
uniform bool noModel;

out vec3 WorldPos;

void main()
{
    WorldPos = aPos;

	mat4 rotView = mat4(mat3(View));

	vec4 clipPos;

	if(noModel){
		clipPos = Proj * rotView * vec4(WorldPos, 1.0);
	}
	else
		clipPos = Proj * rotView * model * vec4(WorldPos, 1.0);

	gl_Position = clipPos.xyww;
}