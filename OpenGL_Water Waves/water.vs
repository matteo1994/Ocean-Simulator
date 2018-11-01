#version 430

layout (location = 0) in vec4 position;

uniform mat4 MVP;
uniform mat4 screenToCamera;
uniform mat4 cameraToWorld;

const float pi = 3.14159;
uniform float waterHeight;
uniform float time;
uniform int numWaves;
uniform float amplitude[8];
uniform float wavelength[8];
uniform float speed[8];
uniform vec2 direction[8];

varying vec3 worldNormal;
varying vec3 eyeNormal;
out float s;

const vec2 p[3] = {vec2(0.7f, 0.6f), vec2(0.2f, -0.3f), vec2(-0.5f, 0.5f)};

float wave(int i, float x, float y) {
    float frequency = 2*pi/wavelength[i];
    float phase = speed[i] * frequency;
    float theta = dot(direction[i], vec2(x, y));
	float eDistance = sqrt(pow(x, 2) + pow(y, 2));  
	float dir = x + (i % 2) * (-y) + ((i + 1) % 2) * y;
    return amplitude[i] * sin(time * frequency - dir * phase);
}

float waveHeight(float x, float y) {
    float height = 0.0;
	float totA = 0.0;
    for (int i = 0; i < numWaves; ++i){
        height += wave(i, x, y);
		totA += amplitude[i];
		}
	s = height/ (totA + numWaves);
	int t = int(time);
	for(int j = 0; j < 0; j++){
		float X = position.x + p[j].x;
		float Z = position.z + p[j].y;

		float hZ = 1/1.0f;
		float amp = 1/4.0f;
		float gamma = 2 * pi * hZ;
		float multipler = 10;
		float damping_ratio = 1/2.0f;

		float eDistance = sqrt(pow(X, 2) + pow(Z, 2));  
		float exponent = -(damping_ratio * eDistance * hZ * multipler);
		float exponential = exp(exponent);
		float exp_decay = gamma * sqrt(1 - pow(damping_ratio,2));	
		
		//This value will be then pass to the fragment shader to visualize it better
		s += amp * exponential * sin(exp_decay * time - (abs(eDistance) * multipler));
	}
    return s;
}

float dWavedx(int i, float x, float y) {
    float frequency = 2*pi/wavelength[i];
    float phase = speed[i] * frequency;
    float theta = dot(direction[i], vec2(x, y));
    float A = amplitude[i] * direction[i].x * frequency;
    return A * cos(theta * frequency + time * phase);
}

float dWavedy(int i, float x, float y) {
    float frequency = 2*pi/wavelength[i];
    float phase = speed[i] * frequency;
    float theta = dot(direction[i], vec2(x, y));
    float A = amplitude[i] * direction[i].y * frequency;
    return A * cos(theta * frequency + time * phase);
}

vec3 waveNormal(float x, float y) {
    float dx = 0.0;
    float dy = 0.0;
    for (int i = 0; i < numWaves; ++i) {
        dx += dWavedx(i, x, y);
        dy += dWavedy(i, x, y);
    }
    vec3 n = vec3(-dx, -dy, 1.0);
    return normalize(n);
}

void main() {

	vec3 cameraDir = normalize((screenToCamera * position).xyz);
    vec3 worldDir = (cameraToWorld * vec4(cameraDir, 0.0)).xyz;

    vec4 pos = position;
    pos.y = waterHeight + waveHeight(pos.x, pos.z);
    //pos = vec4(pos.xyz / pos.w, 1.0);
    worldNormal = waveNormal(pos.x, pos.z);
    //eyeNormal = mat3(transpose(inverse(M))) * worldNormal;
    //gl_Position = gl_ModelViewProjectionMatrix * pos;
    gl_Position = MVP * pos;
}
