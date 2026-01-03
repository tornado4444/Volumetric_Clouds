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
uniform sampler2D CurlNoiseTexture;

const float EARTH_RADIUS = 6378000.0;

float saturate(float x){ return clamp(x,0.0,1.0); }

vec3 skyColor(vec3 rd)
{
    float t = saturate(rd.y * 0.5 + 0.5);
    return mix(vec3(0.03,0.05,0.08), vec3(0.35,0.52,0.85), t);
}

float heightFraction(vec3 worldPos)
{
    float h = length(worldPos - EarthCenter) - EARTH_RADIUS;
    return saturate((h - CloudBottom) / max(CloudTop - CloudBottom, 1.0));
}

float density(vec3 worldPos)
{
    float hf = heightFraction(worldPos);
    if(hf <= 0.0 || hf >= 1.0) return 0.0;

    vec3 rel = worldPos - EarthCenter;
    vec3 p = rel / 8000.0;

    vec2 curl = texture(CurlNoiseTexture, fract(p.xz * 0.05 + vec2(Time*0.01, -Time*0.013))).rg * 2.0 - 1.0;
    p.xz += curl * 0.35;

    float base = texture(lowFrequencyTexture, fract(p * 0.25 + vec3(Time*0.01, 0.0, 0.0))).r;
    float sh = smoothstep(0.55, 0.85, base);
    float hm = smoothstep(0.0, 0.22, hf) * (1.0 - smoothstep(0.75, 1.0, hf));
    sh *= hm;
    sh = max(0.0, sh - 0.02);
    return saturate(sh);
}

void main()
{
    vec2 res = vec2(max(screenWidth,1.0), max(screenHeight,1.0));
    vec2 ndc = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    ndc.x *= res.x / res.y;

    vec3 ro = cameraPosition;
    vec3 rd = normalize(cameraFront * 1.6 + cameraRight * ndc.x + cameraUp * ndc.y);

    vec3 bg = skyColor(rd);

    float t = 0.0;
    float stepSize = 400.0;
    float trans = 1.0;
    vec3 acc = vec3(0.0);

    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    vec3 sunCol = vec3(1.0, 0.95, 0.85);

    for(int i=0;i<64;i++)
    {
        t += stepSize;
        vec3 p = ro + rd * t;
        float d = density(p);
        if(d > 0.0005)
        {
            vec3 src = (sunCol + vec3(0.55,0.60,0.70)*0.25) * d;
            acc += trans * src * stepSize * 0.0008;
            trans *= exp(-d * stepSize * 0.0012);
            if(trans < 0.02) break;
        }
    }

    vec3 col = bg * trans + acc;
    col = clamp(col, 0.0, 1.0);
    color = vec4(col, 1.0);
}
