#version 430 

const float g = 9.8196;
const float eps = 0.001;
const float PI = 3.141592657;
float ellipticalProps = 0.5/1.1;

const float SUN_INTENSITY = 100.0;

const vec3 earthPos = vec3(0.0, 0.0, 6360010.0);

const float SCALE = 1000.0;

const float Rg = 6360.0 * SCALE;
const float Rt = 6420.0 * SCALE;
const float RL = 6421.0 * SCALE;

const float AVERAGE_GROUND_REFLECTANCE = 0.1;

// Rayleigh
const float HR = 8.0 * SCALE;
const vec3 betaR = vec3(5.8e-3, 1.35e-2, 3.31e-2) / SCALE;

// Mie
// DEFAULT
const float HM = 1.2 * SCALE;
const vec3 betaMSca = vec3(4e-3) / SCALE;
const vec3 betaMEx = betaMSca / 0.9;
const float mieG = 0.8;

layout (location = 0) out vec4 FragColor;

uniform mat4 screenToCamera; // screen space to camera space
uniform mat4 cameraToWorld; // camera space to world space
uniform mat4 worldToScreen; // world space to screen space
uniform mat2 worldToWind; // world space to wind space
uniform mat2 windToWorld; // wind space to world space
uniform vec3 worldCamera; // camera position in world space
uniform vec3 worldSunDir; // sun direction in world space


uniform float nbWaves; // number of waves
uniform float[60] h;
uniform float[60] omega;
uniform float[60] kx;
uniform float[60] ky;

uniform sampler2D transmittanceSampler;
uniform sampler2D skyIrradianceSampler;
uniform samplerCube irradianceMap;
uniform samplerCube radianceMap;
uniform sampler2D skyDome;

uniform float heightOffset; // so that surface height is centered around z = 0
uniform vec2 sigmaSqTotal; // total x and y variance in wind space
uniform float time; // current time
uniform float sunLuminance;

// grid cell size in pixels, angle under which a grid cell is seen,
// and parameters of the geometric series used for wavelengths
uniform vec4 lods;

uniform float nyquistMin; // Nmin parameter
uniform float nyquistMax; // Nmax parameter

uniform bool NormalView;
uniform bool reflactance;
uniform bool Irradiance;

vec3 seaColor = vec3(10 /255.0, 40/255.0, 120/255.0); // sea bottom color

in float s;
in float lod;
in vec2 u; // coordinates in wind space used to compute P(u)
in vec3 P; // wave point P(u) in world space
in vec3 _dPdu; // dPdu in wind space, used to compute N
in vec3 _dPdv; // dPdv in wind space, used to compute N
in vec2 _sigmaSq; // variance of unresolved waves in wind space

uniform float hdrExposure;
float R = 0.02; //Fresnel factor for water

vec3 hdr(vec3 L) {
    L = L * hdrExposure;
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.b);
    return L;
}


// optical depth for ray (r,mu) of length d, using analytic formula
// (mu=cos(view zenith angle)), intersections with ground ignored
// H=height scale of exponential density function
float opticalDepth(float H, float r, float mu, float d) {
    float a = sqrt((0.5/H)*r);
    vec2 a01 = a*vec2(mu, mu + d / r);
    vec2 a01s = sign(a01);
    vec2 a01sq = a01*a01;
    float x = a01s.y > a01s.x ? exp(a01sq.x) : 0.0;
    vec2 y = a01s / (2.3193*abs(a01) + sqrt(1.52*a01sq + 4.0)) * vec2(1.0, exp(-d/H*(d/(2.0*r)+mu)));
    return sqrt((6.2831*H)*r) * exp((Rg-r)/H) * (x + dot(y, vec2(1.0, -1.0)));
}

// transmittance(=transparency) of atmosphere for ray (r,mu) of length d
// (mu=cos(view zenith angle)), intersections with ground ignored
// uses analytic formula instead of transmittance texture
vec3 analyticTransmittance(float r, float mu, float d) {
    return exp(- betaR * opticalDepth(HR, r, mu, d) - betaMEx * opticalDepth(HM, r, mu, d));
}

vec2 getTransmittanceUV(float r, float mu) {
    float uR, uMu;
    uR = sqrt((r - Rg) / (Rt - Rg));
    uMu = atan((mu + 0.15) / (1.0 + 0.15) * tan(1.5)) / 1.5;
    return vec2(uMu, uR);
}

vec2 getIrradianceUV(float r, float muS) {
    float uR = (r - Rg) / (Rt - Rg);
    float uMuS = (muS + 0.2) / (1.0 + 0.2);
    return vec2(uMuS, uR);
}

vec3 irradiance(sampler2D sampler, float r, float muS) {
    vec2 uv = getIrradianceUV(r, muS);
    return texture2D(sampler, uv).rgb;
}

// transmittance(=transparency) of atmosphere for infinite ray (r,mu)
// (mu=cos(view zenith angle)), intersections with ground ignored
vec3 transmittance(float r, float mu) {
    vec2 uv = getTransmittanceUV(r, mu);
    return texture2D(transmittanceSampler, uv).rgb;
}

// transmittance(=transparency) of atmosphere for infinite ray (r,mu)
// (mu=cos(view zenith angle)), or zero if ray intersects ground
vec3 transmittanceWithShadow(float r, float mu) {
    return mu < -sqrt(1.0 - (Rg / r) * (Rg / r)) ? vec3(0.0f) : transmittance(r, mu);
}


// incident sky light at given position, integrated over the hemisphere (irradiance)
// r=length(x)
// muS=dot(x,s) / r
vec3 skyIrradiance(float r, float muS) {
    return irradiance(skyIrradianceSampler, r, muS);
}

// incident sun light at given position (radiance)
// r=length(x)
// muS=dot(x,s) / r
vec3 sunRadiance(float r, float muS) {
    return transmittanceWithShadow(r, muS) * SUN_INTENSITY;
}

void sunRadianceAndSkyIrradiance(vec3 worldP, vec3 worldS, out vec3 sunL,  out vec3 skyE)
{
    vec3 worldV = normalize(worldP); // vertical vector
    float r = length(worldP);
    float muS = dot(worldV, worldS);
	sunL = sunRadiance(r, muS);
	if(!Irradiance)
		skyE = skyIrradiance(r, muS); 
}


float effectiveFresnel(float cosThetaV, float sigmaV){
	return R + (1 - R) * pow((1 - cosThetaV), 5 * exp(-2.69 * sigmaV)) / (1 + pow(sigmaV, 1.5) * 22.7);
}

float effectiveFresnel(vec3 V, vec3 N, vec2 sigmaSq) {
    vec2 v = worldToWind * V.xy; // view direction in wind space
    vec2 t = v * v / (1.0 - V.z * V.z); // cos^2 and sin^2 of view direction
    float sigmaV2 = dot(t, sigmaSq); // slope variance in view direction
    return effectiveFresnel(dot(V, N), sqrt(sigmaV2));
}

vec2 reflection(vec3 V, vec3 N){
	vec3 r = 2 * dot(N, V) * N - V; 
	return r.xy / (1 + r.z);
}

vec3 skyRadiance(vec3 V, vec3 N){    
	return (vec4(0.0) + texture2D(skyDome, reflection(V,N) * ellipticalProps + 0.5)).rgb;
}

// ----------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// REFLECTED SUN RADIANCE
// ---------------------------------------------------------------------------

// assumes x>0
float erfc(float x) {
	return 2.0 * exp(-x * x) / (2.319 * x + sqrt(4.0 + 1.52 * x * x));
}


// L, V, N, Tx, Ty in world space
float reflectedSunRadiance(vec3 L, vec3 V, vec3 N, vec3 Tx, vec3 Ty, vec2 sigmaSq, vec2 zeta) {
    vec3 H = normalize(L + V);
    float zetax = zeta.x;
    float zetay = zeta.y;
	
	float p = exp(-0.5 * (pow(zetax,2) / sigmaSq.x + pow(zetay, 2) / sigmaSq.y)) / (sqrt(2.0 * PI) * sqrt(sigmaSq.x * sigmaSq.y));

	vec3 F = 1 / sqrt( 1 + pow(zetax, 2) + pow(zetay, 2)) * vec3(-zetax, -zetay, 1);


	float sinPhiV = dot(V, Tx);
	float cos2PhiV = max(0.0, 1 - pow(sinPhiV, 2));
	

	float cosThetaV = dot(V, N);
	float tanThetaV = sqrt(max(0.0, 1 - pow(cosThetaV, 2))) / cosThetaV;


	float sinPhiL = dot(L, Tx);
	float cos2PhiL = max(0.0, 1 - pow(sinPhiL, 2));

	float cosThetaL = dot(L, N);

	float tanThetaL = sqrt(max(0.0, 1 - pow(cosThetaL, 2))) / cosThetaL;

	vec2 a = vec2(pow(2 * tanThetaV * (sigmaSq.x * cos2PhiV + sigmaSq.y * pow(sinPhiV, 2)), -0.5), pow(2 * tanThetaL * (sigmaSq.x * cos2PhiL + sigmaSq.y * pow(sinPhiL, 2)), -0.5));

	vec2 lambda = vec2((exp(-pow(a.x, 2)) - a.x * sqrt(PI) * erfc(a.x))/ 2 * a.x * sqrt(PI), (exp(-pow(a.y, 2)) - a.y * sqrt(PI) * erfc(a.y))/ 2 * a.y * sqrt(PI));

	float qvn = p / (( 1.0 + lambda.x + lambda.y));

	float fresnel =  0.02 + 0.98 * pow(1.0 - dot(V, H), 5.0);

	return fresnel * qvn / (4.0 * pow(dot(H,N), 4) * dot(V,N));
	
	
}


void main() {
    vec3 dPdu = _dPdu;
    vec3 dPdv = _dPdv;
    vec2 sigmaSq = _sigmaSq;

	
    float iMAX = min(ceil((log2(nyquistMax * lod) - lods.z) * lods.w), nbWaves - 1.0);
    float iMax = floor((log2(nyquistMin * lod) - lods.z) * lods.w);
    float iMin = max(0.0, floor((log2(nyquistMin * lod / lods.x) - lods.z) * lods.w));
    for (float i = iMin; i <= iMAX; i += 1.0) {
         
		int j = int(i);
        vec4 wt = vec4(h[j], omega[j], kx[j], ky[j]);

        float phase = wt.y * time - dot(wt.zw, u);
        float s = sin(phase);
        float c = cos(phase);
        float overk = g / (wt.y * wt.y);

        float wp = smoothstep(nyquistMin, nyquistMax, (2.0 * PI) * overk / lod);
        float wn = smoothstep(nyquistMin, nyquistMax, (2.0 * PI) * overk / lod * lods.x);

        vec3 factor = (1.0 - wp) * wn * wt.x * vec3(wt.zw * overk, 1.0);

        vec3 dPd = factor * vec3(c, c, -s);
        dPdu -= dPd * wt.z;
        dPdv -= dPd * wt.w;

        wt.zw *= overk;
        float kh = i < iMax ? wt.x / overk : 0.0;
        float wkh = (1.0 - wn) * kh;
        sigmaSq -= vec2(wt.z * wt.z, wt.w * wt.w) * (sqrt(1.0 - wkh * wkh) - sqrt(1.0 - kh * kh));
    }
	
    sigmaSq = max(sigmaSq, 2e-5);

    vec3 V = normalize(worldCamera - P);
	vec3 H = normalize(worldSunDir + V);

    vec3 windNormal = normalize(cross(dPdu, dPdv));
    vec3 N = vec3(windToWorld * windNormal.xy, windNormal.z);
    if (dot(V, N) < 0.0) {
        N = reflect(N, V); // reflects backfacing normals
    }

    vec3 Ty = normalize(cross(N, vec3(windToWorld[0], 0.0)));
    vec3 Tx = cross(Ty, N);

	float fresnel = effectiveFresnel(V, N, sigmaSq);

    vec3 Lsun;
    vec3 Esky;
    vec3 Rsky;
    vec3 extinction;
    sunRadianceAndSkyIrradiance(worldCamera + earthPos, worldSunDir, Lsun, Esky);
		
	if(reflactance){
		Rsky = texture(radianceMap, N).rgb;
	}else{
		Rsky = skyRadiance(V, N);
		
	}
	
	if(Irradiance){
		Esky = texture(irradianceMap, N).rgb;
	}

	FragColor = vec4(0.0);

	vec2 zeta = vec2(dot(H, Tx) / dot(H, N), dot(H, Ty) / dot(H, N));

	if(NormalView){
		//FragColor = vec4(zetax, zetay, 0, 0);
		FragColor = vec4(N, 1.0);
	}
    else{
		FragColor.rgb += reflectedSunRadiance(worldSunDir, V, N, Tx, Ty, sigmaSq, zeta) *  Lsun;

		FragColor.rgb += fresnel * Rsky;

		vec3 Lsea = seaColor * Esky;
		FragColor.rgb += (1.0 - fresnel) * Lsea;

		FragColor.rgb = hdr(FragColor.rgb);
	}

}

