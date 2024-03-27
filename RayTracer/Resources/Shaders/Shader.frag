#version 460

layout (location = 0) in vec3 worldNormal;  
layout (location = 1) in vec3 worldPosition;  
layout (location = 2) in vec2 uvFragment;

layout( push_constant ) uniform PushConstants
{
	mat4 model;
	uint albedoId;
	uint metallnessId;
	uint roughnessId;
	uint emissionId;
} constants;

layout (location = 0) out vec4 color;

void main()
{
	const vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
	const vec3 lightPosition = vec3(10.0f, 50.0f, 10.0f);
	vec4 objectColor = vec4(uvFragment, float(constants.albedoId), 1.0f);//texture(Albedo, uvFragment);
	if (objectColor.w < 0.1f)
	{
		discard;
	}
	
    // ambient
    float ambientStrength = 0.5f;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
	vec3 normal = normalize(worldNormal);
    vec3 lightDirection = normalize(lightPosition - worldPosition);
    float diff = max(dot(normal, lightDirection), 0.0f);
    vec3 diffuse = diff * lightColor;
        
    color = vec4((ambient + diffuse) * objectColor.xyz, 1.0f);
}
