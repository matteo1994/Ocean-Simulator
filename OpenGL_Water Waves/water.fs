
#version 430

layout (location = 0) out vec4 FragColor;


uniform samplerCube irradianceMap;


uniform vec3  albedo;  //Sea color
uniform float metallic;
uniform float roughness;
uniform float ao;

in float s;

void main() {
     //vec3 eye = normalize(eyePos - position);
     //vec3 r = reflect(eye, worldNormal);
     //vec4 color = textureCube(envMap, r);

	vec3 N = vec3(1.0f);

	vec3 kS = fresnelSchlick(max(dot(N, V), 0.0), F0);
	vec3 kD = 1.0 - kS;
	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuse    = irradiance * albedo;
	vec3 ambient    = (kD * diffuse) * ao; 

	vec4 color = vec4(5*s, 0.5f, 0.5f, 1.0f);
	color.a = 0.5;
    FragColor = color;
}
