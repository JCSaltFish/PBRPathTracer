#ifndef __shaders_h__
#define __shaders_h__

std::string LoadShader(const std::string& strShaderFilename);
GLuint CreateShader(GLenum eShaderType, const std::string& strShaderFile);
GLuint CreateProgram(const std::vector<GLuint>& shaderList);

const std::string vQuad =
{
	"#version 410\n"

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
	"#version 410\n"

	"uniform sampler2D tex;\n"

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
	"#version 410\n"
	
	"uniform mat4 PV;\n"
	"uniform mat4 M;\n"

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
	"#version 410\n"
	
	"uniform vec3 eyePos;\n"
	"uniform vec3 color;\n"
	"uniform sampler2D normalTex;\n"
	"uniform int normalMap = 0;\n"

	"uniform int pass = 0;\n"
	"uniform int objectId = 0;\n"
	"uniform int elementId = 0;\n"

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
	"		if (normalMap == 1)\n"
	"		{\n"
	"			vec3 bitangentW = normalize(cross(normalW, tangentW));\n"
	"			mat3 TBN = mat3(tangentW, bitangentW, normalW);\n"
	"			vec3 nt = normalize(texture2D(normalTex, texCoord).xyz * 2.0 - 1.0);\n"
	"			n = TBN * nt;\n"
	"		}\n"
	"		vec3 shade = color * max(dot(n, l), 0.0);\n"

	"		fragcolor = vec4(shade, 1.0);\n"
	"	}\n"

	"	else if (pass == 1)\n"
	"		fragcolor = vec4(objectId, elementId, 0.0, 1.0);\n"
	"}\n"
};

#endif
