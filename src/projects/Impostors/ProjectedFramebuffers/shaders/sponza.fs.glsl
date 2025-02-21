#version 430 core

in vec3 position;
in vec3 texCoord;
in vec3 normal;
in vec3 tangent;
in vec3 bitangent;
out vec4 fragmentOutput;

uniform sampler2DArray textures;

uniform mat4 transform;
uniform vec3 lightPosition;

const vec3 viewPosition = vec3( 0.0f );

out float gl_FragDepth;
layout( location = 0 ) out vec4 normalResult;
layout( location = 1 ) out vec4 positionResult;
layout( location = 2 ) out vec4 albedo;

void main () {
	// texture array - use texcoord z value * 2 for color, * 2 + 1 for normals
	vec3 tcNormal = vec3( texCoord.xy, texCoord.z * 2.0f + 1.0f );
	vec3 tcColor = vec3( texCoord.xy, texCoord.z * 2.0f );

	vec4 texRefNormal = texture( textures, tcNormal );
	vec4 texRefColor = texture( textures, tcColor );

	// construct TBN locally, with interpolated normal
	mat3 TBN = mat3( tangent, bitangent, normal );
	vec3 nMappedNormal = normalize( TBN * ( texRefNormal.xyz * 2.0f - 1.0f ) );

	// skip blending, treat as 1-bit alpha
	if( texRefColor.a < 0.01f ) discard;

// Shading ( blinn-phong )
// ====================================================
// ambient term
	vec3 ambient = 0.2f * texRefColor.xyz;
// ====================================================
// diffuse term
	vec3 lightDirection = normalize( lightPosition - position );
	float diff = max( dot( lightDirection, nMappedNormal ), 0.0f );
	vec3 diffuse = diff * texRefColor.xyz;
// ====================================================
// specular term
	vec3 viewDirection = normalize( viewPosition - position );
	vec3 halfwayDirection = normalize( lightDirection + viewDirection );
	float s = pow( max( dot( nMappedNormal, halfwayDirection ), 0.0f ), 32.0f );
	vec3 specular = vec3( 0.3f ) * s; // assuming bright white light color
// ====================================================

	positionResult = vec4( position * 0.001f, 1.0f );

	// normalResult = vec4( normal, 1.0f );
	normalResult = vec4( nMappedNormal, 1.0f );

	albedo = vec4( texRefColor.xyz, 1.0f );
	// albedo = vec4( ambient + diffuse + specular, 1.0f );
}
