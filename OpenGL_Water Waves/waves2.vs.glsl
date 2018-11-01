#version 430

const float g = 9.8196;

const float PI = 3.141592657;

layout (location = 0) in vec4 position;

uniform mat4 screenToCamera; // screen space to camera space
uniform mat4 cameraToWorld; // camera space to world space
uniform mat4 MVP; // world space to screen space
uniform mat2 worldToWind; // world space to wind space
uniform mat2 windToWorld; // wind space to world space
uniform vec3 worldCamera; // camera position in world space
 
uniform float nbWaves; // number of waves
uniform float[60] h;
uniform float[60] omega;
uniform float[60] kx;
uniform float[60] ky;

//uniform sampler1D wavesSampler; // waves parameters (h, omega, kx, ky) in wind space
uniform float heightOffset; // so that surface height is centered around y = 0
uniform vec2 sigmaSqTotal; // total x and y variance in wind space
uniform float time; // current time

// grid cell size in pixels, angle under which a grid cell is seen,
// and parameters of the geometric series used for wavelengths
uniform vec4 lods;

uniform float nyquistMin; // Nmin parameter
uniform float nyquistMax; // Nmax parameter

out float s;
out float lod;
out vec2 u; // coordinates in wind space used to compute P(u)
out vec3 P; // wave point P(u) in world space
out vec3 _dPdu; // dPdu in wind space, used to compute N
out vec3 _dPdv; // dPdv in wind space, used to compute N
out vec2 _sigmaSq; // variance of unresolved waves in wind space

void main() {

	vec4 cameraPos = screenToCamera * position;
    vec4 worldPos = cameraToWorld * cameraPos;

	lowp vec3 cameraDir = normalize((screenToCamera * position).xyz);
    vec3 worldDir = (cameraToWorld * vec4(cameraDir, 0.0)).xyz;
    float t = (heightOffset - worldCamera.z) / worldDir.z;	
    u = worldToWind * (worldCamera.xy + t * worldDir.xy);
	
	vec2 uPos = worldToWind * (worldCamera.xy + t * worldPos.xy);

	vec3 dPdu = vec3(1.0, 0.0, 0.0);
    vec3 dPdv = vec3(0.0, 1.0, 0.0);
    vec2 sigmaSq = sigmaSqTotal;


	lod = - t / worldDir.z * lods.y;

	float waveHeight = 0;
	float waveDrag = 0;
	vec3 waveDisplacement = vec3(0, 0, heightOffset);

	float iMin = max(0.0, floor((log2(nyquistMin * lod) - lods.z) * lods.w));

	for(float j = iMin; j < nbWaves; j+= 1.0f){
		
		int i = int(j);

		float dir = worldPos.z;
		mediump vec2 k = vec2(kx[i], ky[i]);
		float phase = dot(k, u); 

		// 1/k(i) = g / w(i)^2
        float overk = g / pow(omega[i], 2);  

        float wp = smoothstep(nyquistMin, nyquistMax, (2.0 * PI) * overk / lod);
		
		mediump vec3 suppressedH = wp * h[i] * overk * vec3(k, 1.0);
		waveDisplacement += suppressedH * vec3(sin(omega[i] * time - phase), sin(omega[i] * time - phase), cos(omega[i] * time - phase));
	
		mediump vec3 dPd = suppressedH * vec3(cos(omega[i] * time - phase), cos(omega[i] * time - phase), (-sin(omega[i] * time - phase))); 
        dPdu -= dPd * k.x;
        dPdv -= dPd * k.y;

		/* 
		//Calculate the variance along x and y using this formula
		// {[k(x), k(y)](i)}^2 / [k(i)]^2 * ( 1 - sqrt( 1 - k(i)^2*h(i)^2))

		// [k(x), k(y)](i) / ||k||(i)  -> [cos(k(i)), sin(k(i))]  
        k *= overk;

		// k(i)*h(i)
        float kh = k.x / overk;
        sigmaSq -= vec2(k.x * k.x, k.y * k.y) * (1.0 - sqrt(1.0 - kh * kh));
		*/
	}

	worldPos = vec4(windToWorld * (u + waveDisplacement.xy), waveDisplacement.z, 1.0);
	P = worldPos.xyz;


	
	if( t > 0)
		gl_Position = MVP * worldPos;


	_dPdu = dPdu;
    _dPdv = dPdv;
    _sigmaSq = sigmaSq ;

}