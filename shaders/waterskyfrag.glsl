#version 330 core
out vec4 color;

uniform float Time;
uniform float screenWidth;
uniform float screenHeight;

uniform vec3 cameraPosition;
uniform vec3 cameraFront;
uniform vec3 cameraUp;
uniform vec3 cameraRight;

float saturate(float x){ return clamp(x,0.0,1.0); }

float hash(vec2 p){
    p = fract(p*vec2(123.34,456.21));
    p += dot(p,p+45.32);
    return fract(p.x*p.y);
}

float noise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    vec2 u=f*f*(3.0-2.0*f);
    float a=hash(i+vec2(0,0));
    float b=hash(i+vec2(1,0));
    float c=hash(i+vec2(0,1));
    float d=hash(i+vec2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p){
    float f=0.0,a=0.5;
    for(int i=0;i<6;++i){
        f += a*noise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return f;
}

float waves(vec2 xz, float t){
    float h=0.0;
    h += 0.75*sin(dot(xz, vec2(0.020, 0.028)) + t*1.25);
    h += 0.45*sin(dot(xz, vec2(-0.030, 0.018)) + t*1.65);
    h += 0.25*sin(dot(xz, vec2(0.050, -0.016)) + t*2.20);
    h += (fbm(xz*0.08 + vec2(t*0.18, -t*0.14)) - 0.5) * 0.70;
    return h;
}

vec3 skyColor(vec3 rd){
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    float sun = pow(max(dot(rd, lightDir), 0.0), 128.0);
    vec3 base = mix(vec3(0.03,0.05,0.08), vec3(0.35,0.52,0.85), saturate(rd.y*0.5+0.6));
    base += sun * vec3(1.0, 0.85, 0.55) * 0.55;
    return base;
}

void main(){
    vec2 res = vec2(max(screenWidth,1.0), max(screenHeight,1.0));
    vec2 ndc = (gl_FragCoord.xy / res) * 2.0 - 1.0;
    ndc.x *= res.x / res.y;

    vec3 rd = normalize(cameraFront * 1.6 + cameraRight * ndc.x + cameraUp * ndc.y);
    vec3 ro = cameraPosition;

    float waterY = 0.0;

    if(rd.y >= -1e-5){
        color = vec4(skyColor(rd), 1.0);
        return;
    }

    float tHit = (waterY - ro.y) / rd.y;
    if(tHit <= 0.0){
        color = vec4(skyColor(rd), 1.0);
        return;
    }

    vec3 p = ro + rd * tHit;
    vec2 xz = p.xz;

    float time = Time * 0.85;

    float eps = 0.25;
    float h0 = waves(xz, time);
    float hx = waves(xz + vec2(eps, 0.0), time);
    float hz = waves(xz + vec2(0.0, eps), time);

    float dhdx = (hx - h0) / eps;
    float dhdz = (hz - h0) / eps;

    vec3 n = normalize(vec3(-dhdx*2.2, 1.0, -dhdz*2.2));

    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.2));
    vec3 refl = skyColor(reflect(rd, n));

    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
    fres = mix(0.03, 1.0, fres);

    float dist = tHit;
    vec3 waterDeep = vec3(0.01, 0.06, 0.09);
    vec3 waterShallow = vec3(0.03, 0.20, 0.28);
    float depthFactor = saturate(1.0 - exp(-dist * 0.0035));
    vec3 waterCol = mix(waterShallow, waterDeep, depthFactor);

    float spec = pow(max(dot(reflect(-lightDir, n), -rd), 0.0), 64.0);
    vec3 sunSpec = vec3(1.0, 0.95, 0.75) * spec * 1.25;

    float slope = length(vec2(dhdx, dhdz));
    float foam = smoothstep(0.35, 0.95, slope);
    foam *= smoothstep(0.0, 250.0, dist);
    vec3 foamCol = vec3(0.85, 0.90, 0.92);

    vec3 col = mix(waterCol, refl, fres);
    col += sunSpec;
    col = mix(col, foamCol, foam * 0.35);

    float haze = saturate(exp(-abs(rd.y) * 2.2));
    vec3 horizonFog = mix(vec3(0.55,0.62,0.70), skyColor(rd), 0.55);
    col = mix(col, horizonFog, 0.28 * haze);

    color = vec4(clamp(col, 0.0, 1.0), 1.0);
}
