#version 410

layout (location = 0) in vec4 iPosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()					
{
	gl_Position = proj * view * model * iPosition; // standard vertex out          
}
