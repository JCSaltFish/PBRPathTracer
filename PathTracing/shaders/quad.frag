#version 410

uniform sampler2D tex;

in vec2 tex_coord;
out vec4 fragcolor;

void main()
{
	vec4 res = texelFetch(tex, ivec2(gl_FragCoord), 0);
	fragcolor = texelFetch(tex, ivec2(gl_FragCoord), 0);
}
