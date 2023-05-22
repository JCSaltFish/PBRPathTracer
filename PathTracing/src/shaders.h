#ifndef __shaders_h__
#define __shaders_h__

std::string LoadShader(const std::string& strShaderFilename);
GLuint CreateShader(GLenum eShaderType, const std::string& strShaderFile);
GLuint CreateProgram(const std::vector<GLuint>& shaderList);

const std::string vQuad =
{
	"#version 430\n"

	"out vec2 tex_coord;\n"

	"const vec4 quad[4] = vec4[]\n"
	"(\n"
	"	vec4(-1.0, 1.0, 0.0, 1.0),\n"
	"	vec4(-1.0, -1.0, 0.0, 1.0),\n"
	"	vec4(1.0, 1.0, 0.0, 1.0),\n"
	"	vec4(1.0, -1.0, 0.0, 1.0)\n"
	");\n"

	"void main()\n"
	"{\n"
	"	gl_Position = quad[gl_VertexID];\n"
	"	tex_coord = 0.5 * (quad[gl_VertexID].xy + vec2(1.0));\n"
	"}\n"
};

const std::string fQuad =
{
	"#version 430\n"

	"layout(location = 0) uniform sampler2D tex;\n"

	"in vec2 tex_coord;\n"
	"out vec4 fragcolor;\n"

	"void main()\n"
	"{\n"
	"	vec4 res = texelFetch(tex, ivec2(gl_FragCoord), 0);\n"
	"	fragcolor = texelFetch(tex, ivec2(gl_FragCoord), 0);\n"
	"}\n"
};

const std::string vPrev =
{
	"#version 430\n"
	
	"layout(location = 0) uniform mat4 PV;\n"
	"layout(location = 1) uniform mat4 M;\n"

	"in vec3 iPos;\n"
	"in vec3 iNormal;\n"
	"in vec3 iTangent;\n"
	"in vec2 iTexCoord;\n"

	"out vec3 posW;\n"
	"out vec3 normalW;\n"
	"out vec3 tangentW;\n"
	"out vec2 texCoord;\n"

	"void main()\n"
	"{\n"
	"	posW = (M * vec4(iPos, 1.0)).xyz;\n"
	"	normalW = normalize(M * vec4(iNormal, 0.0)).xyz;\n"
	"	tangentW = normalize(M * vec4(iTangent, 0.0)).xyz;\n"
	"	texCoord = iTexCoord;\n"

	"	gl_Position = PV * M * vec4(iPos, 1.0);\n"
	"}\n"
};

const std::string fPrev =
{
	"#version 430\n"

	"layout(location = 2) uniform vec3 eyePos;\n"

	"layout(binding = 0) uniform MaterialUniforms\n"
	"{\n"
	"	vec4 color;\n"
	"	vec4 specular;\n"

	"	float roughness;\n"
	"	float reflectiveness;\n"

	"	int diffuseMap;\n"
	"	int normalMap;\n"
	"	int roughnessMap;\n"
	"	int metallicMap;\n"
	"} Material;\n"

	"layout(location = 3) uniform sampler2D diffuseTex;\n"
	"layout(location = 4) uniform sampler2D normalTex;\n"
	"layout(location = 5) uniform sampler2D roughnessTex;\n"
	"layout(location = 6) uniform sampler2D metallicTex;\n"

	"layout(location = 7) uniform int pass = 0;\n"
	"layout(location = 8) uniform int objectId = 0;\n"
	"layout(location = 9) uniform int elementId = 0;\n"
	"layout(location = 10) uniform int highlight = 0;\n"

	"in vec3 posW;\n"
	"in vec3 normalW;\n"
	"in vec3 tangentW;\n"
	"in vec2 texCoord;\n"

	"out vec4 fragcolor;\n"

	"void main()\n"
	"{\n"
	"	if (pass == 0)\n"
	"	{\n"
	"		vec3 l = normalize(eyePos - posW);\n"
	"		vec3 n = normalW;\n"
	"		if (dot(n, l) < 0.0)\n"
	"			n = -n;\n"
	"		if (Material.normalMap == 1)\n"
	"		{\n"
	"			vec3 bitangentW = normalize(cross(normalW, tangentW));\n"
	"			mat3 TBN = mat3(tangentW, bitangentW, normalW);\n"
	"			vec3 nt = normalize(texture2D(normalTex, texCoord).rgb * 2.0 - 1.0);\n"
	"			n = TBN * nt;\n"
	"		}\n"

	"		float roughness = Material.roughness;\n"
	"		if (Material.roughnessMap == 1)\n"
	"			roughness = texture2D(roughnessTex, texCoord).r;\n"
	"		float reflectiveness = Material.reflectiveness;\n"
	"		if (Material.metallicMap == 1)\n"
	"			reflectiveness = texture2D(metallicTex, texCoord).r;\n"

	"		vec3 diffuse = Material.color.rgb;\n"
	"		if (Material.diffuseMap == 1 && highlight != 1)\n"
	"			diffuse = texture2D(diffuseTex, texCoord).rgb;\n"
	"		diffuse *= max(dot(n, l), 0.0);\n"

	"		vec3 specular = Material.specular.rgb;\n"
	"		specular *= pow(max(dot(n, l), 0.), 128. * (1. - Material.roughness));\n"
	"		specular *= max(dot(n, l), 0.0);\n"

	"		vec3 shade = diffuse * (1. - Material.reflectiveness) + specular * Material.reflectiveness;\n"
	"		if (highlight == 1)\n"
	"			shade = diffuse;\n"

	"		fragcolor = vec4(shade, 1.);\n"
	"	}\n"

	"	else if (pass == 1)\n"
	"		fragcolor = vec4(objectId, elementId, 0.0, 1.0);\n"
	"}\n"
};

#endif
