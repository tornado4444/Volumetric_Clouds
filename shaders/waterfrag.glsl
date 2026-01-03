#version 330 core
out vec4 color;

uniform float Time;
uniform float screenWidth;
uniform float screenHeight;

uniform vec3 cameraPosition;
uniform vec3 cameraFront;
uniform vec3 cameraUp;
uniform vec3 cameraRight;

float saturate(float x){ return clamp(x, 0.0, 1.0); }

vec3 skyColor(vec3 rd)
{
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    float sun = pow(max(dot(rd, lightDir), 0.0), 256.0);
    float t = saturate(rd.y * 0.5 + 0.5);
    vec3 base = mix(vec3(0.03,0.05,0.08), vec3(0.35,0.52,0.85), t);
    base += sun * vec3(1.0, 0.9, 0.65) * 0.55;
    return base;
}

float hash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f*f*(3.0-2.0*f);
    float a = hash21(i + vec2(0,0));
    float b = hash21(i + vec2(1,0));
    float c = hash21(i + vec2(0,1));
    float d = hash21(i + vec2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p)
{
    float f = 0.0;
    float a = 0.5;
    for(int i=0;i<5;i++)
    {
        f += a * noise(p);
        p *= 2.02;
        a *= 0.5;
    }
    return f;
}

void oceanWaves(in vec2 xz, in float t, in float ampScale, out float h, out vec2 grad)
{
    vec2 g = vec2(0.0);
    float height = 0.0;

    vec2 d1 = normalize(vec2( 0.80, 0.60));
    vec2 d2 = normalize(vec2(-0.55, 0.83));
    vec2 d3 = normalize(vec2( 0.20,-0.98));

    float A1 = 0.60 * ampScale;
    float A2 = 0.32 * ampScale;
    float A3 = 0.18 * ampScale;

    float L1 = 240.0;
    float L2 = 130.0;
    float L3 =  65.0;

    float k1 = 6.2831853 / L1;
    float k2 = 6.2831853 / L2;
    float k3 = 6.2831853 / L3;

    float s1 = 0.55;
    float s2 = 0.90;
    float s3 = 1.25;

    float p1 = k1 * dot(d1, xz) + t * s1;
    float p2 = k2 * dot(d2, xz) + t * s2;
    float p3 = k3 * dot(d3, xz) + t * s3;

    float c1 = cos(p1), c2 = cos(p2), c3 = cos(p3);
    float sn1 = sin(p1), sn2 = sin(p2), sn3 = sin(p3);

    height += A1 * sn1;
    height += A2 * sn2;
    height += A3 * sn3;

    g += A1 * k1 * d1 * c1;
    g += A2 * k2 * d2 * c2;
    g += A3 * k3 * d3 * c3;

    float rip = (fbm(xz * 0.015 + vec2(t*0.04, -t*0.03)) - 0.5);
    height += rip * (0.18 * ampScale);

    h = height;
    grad = g;
}

void main()
{
    vec2 res = vec2(max(screenWidth,1.0), max(screenHeight,1.0));
    vec2 ndc = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    ndc.x *= res.x / res.y;

    vec3 ro = cameraPosition;
    vec3 rd = normalize(cameraFront * 1.6 + cameraRight * ndc.x + cameraUp * ndc.y);

    float waterY = 0.0;

    if (rd.y >= -1e-6)
    {
        color = vec4(skyColor(rd), 1.0);
        return;
    }

    float tHit = (waterY - ro.y) / rd.y;
    if (tHit <= 0.0)
    {
        color = vec4(skyColor(rd), 1.0);
        return;
    }

    vec3 p = ro + rd * tHit;
    vec2 xz = p.xz;

    float dist = tHit;
    float ampScale = mix(1.0, 0.22, saturate(dist * 0.0012));

    float h;
    vec2 grad;
    oceanWaves(xz, Time * 0.85, ampScale, h, grad);
    vec3 n = normalize(vec3(-grad.x, 1.0, -grad.y));

    vec3 refl = skyColor(reflect(rd, n));
    float NdotV = saturate(dot(n, normalize(-rd)));
    float fres = 0.02 + (1.0 - 0.02) * pow(1.0 - NdotV, 5.0);

    vec3 waterDeep = vec3(0.01, 0.06, 0.09);
    vec3 waterShallow = vec3(0.03, 0.18, 0.25);
    float depthFactor = saturate(1.0 - exp(-dist * 0.0024));
    vec3 waterCol = mix(waterShallow, waterDeep, depthFactor);

    vec3 col = mix(waterCol, refl, fres);
    col = clamp(col, 0.0, 1.0);
    color = vec4(col, 1.0);
}
