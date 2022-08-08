#version 410

uniform mat4 PV;
uniform mat4 M;

in vec3 iPos;
in vec3 iNormal;
in vec3 iTangent;
in vec2 iTexCoord;

out vec3 posW;
out vec3 normalW;
out vec3 tangentW;
out vec2 texCoord;

void main()
{
	posW = (M * vec4(iPos, 1.0)).xyz;
	normalW = normalize(M * vec4(iNormal, 0.0)).xyz;
	tangentW = normalize(M * vec4(iTangent, 0.0)).xyz;
	texCoord = iTexCoord;

	gl_Position = PV * M * vec4(iPos, 1.0);
}
