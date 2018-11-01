#version 430

const float g = 9.81;

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

uniform sampler1D wavesSampler; // waves parameters (h, omega, kx, ky) in wind space
uniform float heightOffset; // so that surface height is centered around z = 0
uniform vec2 sigmaSqTotal; // total x and y variance in wind space
uniform float time; // current time

// grid cell size in pixels, angle under which a grid cell is seen,
// and parameters of the geometric series used for wavelengths
uniform vec4 lods;

uniform float nyquistMin; // Nmin parameter
uniform float nyquistMax; // Nmax parameter

uniform vec3 seaColor; // sea bottom color

varying float lod;

varying vec2 u; // coordinates in wind space used to compute P(u)
varying vec3 P; // wave point P(u) in world space
varying vec3 _dPdu; // dPdu in wind space, used to compute N
varying vec3 _dPdv; // dPdv in wind space, used to compute N
varying vec2 _sigmaSq; // variance of unresolved waves in wind space

out float sC;

void main() {
    gl_Position = position;
	sC = 1.0f;

    vec3 cameraDir = normalize((screenToCamera * position).xyz);
    vec3 worldDir = (cameraToWorld * vec4(cameraDir, 0.0)).xyz;
    float t = (heightOffset - worldCamera.z) / worldDir.z;	
	
	

    u = worldToWind * (worldCamera.xy + t * worldDir.xy);
    vec3 dPdu = vec3(1.0, 0.0, 0.0);
    vec3 dPdv = vec3(0.0, 1.0, 0.0);
    vec2 sigmaSq = sigmaSqTotal;
	
    lod = - t / worldDir.z * lods.y; // size in meters of one grid cell, projected on the sea surface

    vec3 dP = vec3(0.0, 0.0, heightOffset);
    float iMin = max(0.0, floor((log2(nyquistMin * lod) - lods.z) * lods.w));

	
    for (float i = iMin; i < nbWaves; i += 1.0) {
        //wt = texture1DLod(wavesSampler, (i + 0.5) / nbWaves, 0.0);
		int j = int((i + 0.5) / nbWaves);
        vec4 wt = vec4(h[j], omega[j], kx[j], ky[j]);


		//w(i)*t - K.x   -> argument of the sin and cosine functions K is a vector
		//along which make the displacement
		float phase = wt.y * time - dot(wt.zw, u);
        float s = sin(phase);
        float c = cos(phase);


		// 1/k(i) = g / w(i)^2
        float overk = g / (wt.y * wt.y);  

        float wp = smoothstep(nyquistMin, nyquistMax, (2.0 * PI) * overk / lod);

        vec3 factor = wp * wt.x * vec3(wt.zw * overk, 1.0);
        dP += factor * vec3(s, s, c);

        vec3 dPd = factor * vec3(c, c, -s);
        dPdu -= dPd * wt.z;
        dPdv -= dPd * wt.w;

		//Calculate the variance along x and y using this formula
		// {[k(x), k(y)](i)}^2 / [k(i)]^2 * ( 1 - sqrt( 1 - k(i)^2*h(i)^2))

		// [k(x), k(y)](i) / ||k||(i)  -> [cos(k(i)), sin(k(i))]  
        wt.zw *= overk;

		// k(i)*h(i)
        float kh = wt.x / overk;
        sigmaSq -= vec2(wt.z * wt.z, wt.w * wt.w) * (1.0 - sqrt(1.0 - kh * kh));
    }

	//Project the point back to World cordinate as WToWi[x, y] + sum(trochoids)
    P = vec3(windToWorld * (u + dP.xy), dP.z);

	

	//Project the point back to Screen
    if (t > 0.0) {
        gl_Position = MVP * vec4(P, 1.0);
		sC = gl_Position.z;
    }
	
    _dPdu = dPdu;
    _dPdv = dPdv;
    _sigmaSq = sigmaSq;
}
