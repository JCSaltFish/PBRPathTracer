#version 410

uniform sampler2D tex;

in vec2 tex_coord;
out vec4 fragcolor;

void main()
{
	fragcolor = texelFetch(tex, ivec2(gl_FragCoord), 0);
}
