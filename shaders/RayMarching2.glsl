#version 330 core
out vec4 color;

uniform float Time;
uniform float screenWidth;
uniform float screenHeight;

uniform vec3 cameraPosition;
uniform vec3 cameraFront;
uniform vec3 cameraUp;
uniform vec3 cameraRight;

uniform vec3 EarthCenter;

uniform float CloudBottom;
uniform float CloudTop;

uniform sampler3D lowFrequencyTexture;
uniform sampler3D highFrequencyTexture;
uniform sampler2D WeatherTexture;
uniform sampler2D CurlNoiseTexture;

const float EARTH_RADIUS = 6378000.0;

float saturate(float x){ return clamp(x,0.0,1.0); }

float heightFraction(vec3 worldPos)
{
    float h = length(worldPos - EarthCenter) - EARTH_RADIUS;
    return saturate((h - CloudBottom) / max(CloudTop - CloudBottom, 1.0));
}

float sampleCloudDensity(vec3 worldPos)
{
    float hf = heightFraction(worldPos);
    if(hf <= 0.0 || hf >= 1.0) return 0.0;

    vec3 rel = worldPos - EarthCenter;
    vec3 p = rel / 8000.0;

    vec2 curl = texture(CurlNoiseTexture, fract(p.xz * 0.05 + vec2(Time*0.01, -Time*0.013))).rg * 2.0 - 1.0;
    p.xz += curl * 0.35;

    vec4 lf = texture(lowFrequencyTexture, fract(p * 0.25 + vec3(Time*0.01, 0.0, 0.0)));
    float base = lf.r;
    float worleyFBM = lf.g * 0.625 + lf.b * 0.25 + lf.a * 0.125;
    worleyFBM = saturate(worleyFBM);

    float shape = smoothstep(0.52 - 0.30*worleyFBM, 0.84, base);

    vec2 wuv = fract(rel.xz / 200000.0 + 0.5);
    float coverage = texture(WeatherTexture, wuv).r;
    coverage = mix(0.20, 0.70, coverage);

    float heightMask = smoothstep(0.0, 0.22, hf) * (1.0 - smoothstep(0.70, 1.0, hf));
    shape *= heightMask;

    shape = saturate((shape - (1.0 - coverage)) / max(coverage, 1e-4));

    float hfNoise = texture(highFrequencyTexture, fract(p * 0.9 + vec3(0.0, Time*0.02, 0.0))).r;
    shape -= (1.0 - hfNoise) * 0.26;

    shape = max(0.0, shape - 0.018);
    return saturate(shape);
}

void main()
{
    vec2 res = vec2(max(screenWidth,1.0), max(screenHeight,1.0));
    vec2 ndc = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    ndc.x *= res.x / res.y;

    vec3 ro = cameraPosition;
    vec3 rd = normalize(cameraFront * 1.6 + cameraRight * ndc.x + cameraUp * ndc.y);

    vec3 p = ro + rd * 40000.0;
    float d = sampleCloudDensity(p);
    color = vec4(vec3(d), 1.0);
}
