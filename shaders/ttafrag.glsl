#version 330 core
out vec4 color;
in vec2 vUV;

uniform sampler2D uCurrent;
uniform sampler2D uHistory;

uniform vec2 uResolution;
uniform float uAlpha;

void main()
{
    vec3 cur = texture(uCurrent, vUV).rgb;
    vec3 hist = texture(uHistory, vUV).rgb;
    vec3 outc = mix(cur, hist, clamp(uAlpha, 0.0, 0.99));
    color = vec4(outc, 1.0);
}
