#version 410

uniform vec3 eyePos;
uniform vec3 color;
uniform sampler2D normalTex;
uniform int normalMap = 0;

in vec3 posW;
in vec3 normalW;
in vec3 tangentW;
in vec2 texCoord;

out vec4 fragcolor;

void main()
{
	vec3 l = normalize(eyePos - posW);
	vec3 n = normalW;
	if (dot(n, l) < 0.0f)
		n = -n;
	if (normalMap == 1)
	{
		vec3 bitangentW = normalize(cross(normalW, tangentW));
		mat3 TBN = mat3(tangentW, bitangentW, normalW);
		vec3 nt = normalize(texture2D(normalTex, texCoord).xyz * 2.0 - 1.0);
		n = TBN * nt;
	}
	vec3 shade = color * max(dot(n, l), 0.0);

	fragcolor = vec4(shade, 1.0);
}
