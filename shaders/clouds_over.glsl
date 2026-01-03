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
uniform vec2 HaltonSequence;

uniform float CloudBottom;
uniform float CloudTop;

uniform sampler3D lowFrequencyTexture;
uniform sampler3D highFrequencyTexture;
uniform sampler2D WeatherTexture;
uniform sampler2D CurlNoiseTexture;

const float PI = 3.14159265;
const float EARTH_RADIUS = 6378000.0;

float saturate(float x){ return clamp(x,0.0,1.0); }

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

float phaseHG(float g, float cosT)
{
    float g2 = g*g;
    float denom = pow(1.0 + g2 - 2.0*g*cosT, 1.5);
    return (1.0 - g2) / max(4.0 * PI * denom, 1e-6);
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

vec3 tonemap(vec3 x)
{
    return x / (1.0 + x);
}

void main()
{
    vec2 res = vec2(max(screenWidth, 1.0), max(screenHeight, 1.0));
    vec2 uv = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    uv.x *= res.x / res.y;

    vec3 ro = cameraPosition;
    vec3 rd = normalize(cameraFront * 1.6 + cameraRight * uv.x + cameraUp * uv.y);

    float rOuter = EARTH_RADIUS + CloudTop;

    float tAtm0, tAtm1;
    if(!sphereIntersect(ro, rd, EarthCenter, rOuter, tAtm0, tAtm1))
    {
        color = vec4(0.0);
        return;
    }

    float t0 = max(tAtm0, 0.0);
    float t1 = tAtm1;

    int steps = 84;
    float segLen = max(t1 - t0, 0.0);
    float stepSize = segLen / float(steps);

    float j = fract(dot(HaltonSequence, vec2(0.754877, 0.569840)) + 0.5);
    t0 += j * stepSize;

    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    vec3 sunCol = vec3(1.0, 0.95, 0.85);

    float trans = 1.0;
    vec3 accum = vec3(0.0);

    const float EXT = 0.0012;
    const float SCA = 0.0010;

    for(int i = 0; i < steps; ++i)
    {
        float t = t0 + (float(i) + 0.5) * stepSize;
        vec3 p = ro + rd * t;

        float dens = sampleCloudDensity(p);
        if(dens <= 0.0005) continue;

        float shadow = 0.0;
        vec3 lp = p;
        float lStep = 350.0;
        for(int k = 0; k < 8; ++k)
        {
            lp += lightDir * lStep;
            shadow += sampleCloudDensity(lp);
        }
        float lightTrans = exp(-shadow * 1.35);

        float cosT = dot(rd, lightDir);
        float ph = phaseHG(0.60, cosT);
        ph = mix(ph, 1.0 / (4.0 * PI), 0.10);

        vec3 ambient = vec3(0.55, 0.60, 0.70) * 0.030;

        vec3 src = (sunCol * lightTrans * ph + ambient) * dens;

        accum += trans * src * stepSize * SCA;
        trans *= exp(-dens * stepSize * EXT);

        if(trans < 0.01) break;
    }

    float alpha = saturate(1.0 - trans);

    vec3 rgb = tonemap(accum);
    rgb = clamp(rgb, 0.0, 1.0);

    rgb *= alpha;

    color = vec4(rgb, alpha);
}
