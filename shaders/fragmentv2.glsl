#version 330 core
out vec4 color;

uniform float screenWidth;
uniform float screenHeight;

uniform vec3 cameraFront;
uniform vec3 cameraUp;
uniform vec3 cameraRight;

float saturate(float x){ return clamp(x,0.0,1.0); }

vec3 skyColor(vec3 rd)
{
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    float sun = pow(max(dot(rd, lightDir), 0.0), 256.0);
    float t = saturate(rd.y * 0.5 + 0.5);
    vec3 base = mix(vec3(0.03,0.05,0.08), vec3(0.35,0.52,0.85), t);
    base += sun * vec3(1.0, 0.9, 0.65) * 0.5;
    return base;
}

void main()
{
    vec2 res = vec2(max(screenWidth,1.0), max(screenHeight,1.0));
    vec2 ndc = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    ndc.x *= res.x / res.y;
    vec3 rd = normalize(cameraFront * 1.6 + cameraRight * ndc.x + cameraUp * ndc.y);
    color = vec4(skyColor(rd), 1.0);
}
