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

const float PI = 3.14159265;
const float EARTH_RADIUS = 6378000.0;

float saturate(float x){ return clamp(x,0.0,1.0); }

vec3 skyColor(vec3 rd)
{
    float t = saturate(rd.y * 0.5 + 0.5);
    return mix(vec3(0.03,0.05,0.08), vec3(0.35,0.52,0.85), t);
}

bool sphereIntersect(vec3 ro, vec3 rd, vec3 c, float r, out float t0, out float t1)
{
    vec3 oc = ro - c;
    float b = dot(oc, rd);
    float c2 = dot(oc, oc) - r*r;
    float h = b*b - c2;
    if(h < 0.0) return false;
    h = sqrt(h);
    t0 = -b - h;
    t1 = -b + h;
    return true;
}

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

    float rOuter = EARTH_RADIUS + CloudTop;

    float t0, t1;
    if(!sphereIntersect(ro, rd, EarthCenter, rOuter, t0, t1))
    {
        color = vec4(skyColor(rd), 1.0);
        return;
    }

    t0 = max(t0, 0.0);

    int steps = 80;
    float stepSize = max(t1 - t0, 0.0) / float(steps);

    float trans = 1.0;
    vec3 acc = vec3(0.0);

    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    vec3 sunCol = vec3(1.0, 0.95, 0.85);

    for(int i=0;i<steps;i++)
    {
        float t = t0 + (float(i)+0.5)*stepSize;
        vec3 p = ro + rd*t;
        float d = sampleCloudDensity(p);
        if(d <= 0.0005) continue;

        float shadow = 0.0;
        vec3 lp = p;
        for(int k=0;k<8;k++)
        {
            lp += lightDir * 350.0;
            shadow += sampleCloudDensity(lp);
        }
        float lightTrans = exp(-shadow * 1.35);

        vec3 src = (sunCol * lightTrans + vec3(0.55,0.60,0.70)*0.25) * d;

        acc += trans * src * stepSize * 0.0009;
        trans *= exp(-d * stepSize * 0.0012);
        if(trans < 0.02) break;
    }

    vec3 bg = skyColor(rd);
    vec3 col = bg * trans + acc;
    col = clamp(col, 0.0, 1.0);
    color = vec4(col, 1.0);
}
