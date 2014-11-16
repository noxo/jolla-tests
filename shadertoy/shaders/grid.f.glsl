#ifdef GL_ES
precision highp float;
#endif

// Author: https://www.shadertoy.com/view/MdBSzV

const float HALF_PI = 1.57079632679;

uniform vec2 resolution;
uniform float time;
uniform vec4 mouse;

const vec3 colorOne = vec3(0.0, 0.0, 0.5);
const vec3 colorTwo = vec3(0.0, 0.0, 1.0);
const vec3 colorThree = vec3(0., 0.0, 0.0);

vec3 normal(vec2, vec2, float);
vec2 rot(vec2, float);
float sampleGrid(vec2, vec2);
float warp(vec2, float);
float grid(vec2);

float warp(vec2 dif, float rad) {
    //float dist = clamp(length(dif), 0.0, rad);
    //return pow(cos(dist * HALF_PI / rad), 2.0);
    
    return sqrt(1.0 - pow(clamp(length(dif) / rad, 0.0, 1.0), 2.0));
}

float grid(vec2 uv) {
    vec2 v2g = vec2(cos(uv) * 0.5 + 0.5);
    return max(v2g.x, v2g.y);
}

//adapted from 4rknova's shader @ https://www.shadertoy.com/view/4ss3W7#
vec3 normal(vec2 uv, vec2 mp, float a) {
    vec2 offsetX = vec2(  a, 0.0) / resolution.xy;
    vec2 offsetY = vec2(0.0, 1.0) / resolution.xy;

	float R = sampleGrid(uv + offsetX, mp);//7
	float L = sampleGrid(uv - offsetX, mp);//1
	float D = sampleGrid(uv + offsetY, mp);//5
	float U = sampleGrid(uv - offsetY, mp);//3

	float X = (L-R) * 0.5;
	float Y = (U-D) * 0.5;

	return normalize(vec3(X, Y, 0.01));
}

float sampleGrid(vec2 uv, vec2 mp) {
    vec2 dif = mp - uv;
    float p = warp(dif, 0.5);
    vec2 dp = dif * p * 0.5;
    
    vec2 uvf = (uv + dp);
    vec2 uvb = (uv - dp);
    
    float f = pow(grid(uvf * 25.0), 10.0) * 0.25 + 0.625;
    float b = pow(grid(uvb * 25.0), 10.0) * 0.25 + 0.125;

    f *= step(0.75, f);
    b *= step(0.25, b);
    
    return max(f, b);
}

vec2 rot(vec2 old, float ang) {
    float c = cos(ang);
    float s = sin(ang);
    vec2 new = vec2(0.0);
    new.x = c * old.x + s * old.y;
    new.y = -s * old.x + c * old.y;
    return new;
}

vec3 getCol(vec2 uv, vec2 mp, float a) {
    float b = sampleGrid(uv, mp); 
    //vec3 n = normal(uv, mp, a);

    //vec3 c = (b > 0.0 ? (b < 0.5 ? colorTwo : colorOne) : colorThree);
    vec3 c = (b > 0.0 ? mix(colorOne, colorTwo, step(0.5, b)) : colorThree);

	//vec3 lightDif = vec3(mp - uv, 0.25);
    //float light = inversesqrt(length(lightDif)) * dot(n, normalize(lightDif));
	//c *= light;
    return c;
}

vec3 edge(vec2 uv, vec2 mp, float a) {
    vec3 c[9];
	for (int i=0; i < 3; ++i)
	{
		for (int j=0; j < 3; ++j)
		{
            vec2 os = vec2(i-1,j-1);
            vec2 p = (gl_FragCoord.xy + os) / resolution.xy * 2.0 - 1.0;
            p.x *= a;
			c[3*i+j] = getCol(p, mp, a);
		}
	}
	
	vec3 Lx = 2.0*(c[7]-c[1]) + c[6] + c[8] - c[2] - c[0];
	vec3 Ly = 2.0*(c[3]-c[5]) + c[6] + c[0] - c[2] - c[8];
	return sqrt(Lx*Lx+Ly*Ly);
}

void main(void)
{
    float a = resolution.x / resolution.y;
    
    vec2 uv = gl_FragCoord.xy / resolution.xy;
    vec2 mp = vec2(0.0);
    
    if(mouse.z > 0.0) {
        mp = mouse.xy / resolution.xy;
    } else {
        mp = vec2(cos(time), sin(time));
        mp *= 0.25;
        mp += 0.5;
    }
    
    uv = uv * 2.0 - 1.0;
    mp = mp * 2.0 - 1.0;
    uv.x *= a;
    mp.x *= a;
    
    //float ang = -iGlobalTime * 0.1;
    //uv = rot(uv, ang);
    //mp = rot(mp, ang);

	//float e = length(edge(uv, mp, a)) / 3.0;
    vec3 c = getCol(uv, mp, a);
    
	gl_FragColor = vec4(c, 1.0);
}
