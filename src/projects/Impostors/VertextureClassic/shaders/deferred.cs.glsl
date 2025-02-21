#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform sampler2D depthTexture;
uniform sampler2D colorTexture;
uniform sampler2D normalTexture;
uniform sampler2D positionTexture;

uniform vec2 resolution;
uniform mat3 trident;

// from the old shaders
uniform int lightCount;

struct light_t {
	vec4 position;
	vec4 color;
};

layout( binding = 4, std430 ) buffer lightDataBuffer {
	light_t lightData[];
};

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uvec3 blueNoise = uvec3( imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).r * 0.0618f );

	vec2 sampleLocation = ( vec2( writeLoc ) + vec2( 0.5f ) ) / resolution;
	sampleLocation.y = 1.0f - sampleLocation.y;
	vec4 color = texture( colorTexture, sampleLocation );
	vec4 depth = texture( depthTexture, sampleLocation );
	vec4 normal = texture( normalTexture, sampleLocation );
	vec4 position = texture( positionTexture, sampleLocation );

	// geo has been written this frame
	if ( depth.r != 1.0f ) {

		// lighting
		const mat3 inverseTrident = inverse( trident );
		// const float maxDistFactor = 100.0f;
		const float maxDistFactor = 10000000.0f;
		const vec3 eyePosition = vec3( 0.0f, 0.0f, -5.0f );
		const vec3 viewVector = inverseTrident * normalize( eyePosition - position.xyz );
		vec3 lightContribution = vec3( 0.0f );

		// pack into one of the remaining fields ( or add additional rendered target with material properties )
		const float roughness = 1.0f;

		for ( int i = 0; i < lightCount; i++ ) {
			// phong setup
			const vec3 lightLocation = lightData[ i ].position.xyz;
			const vec3 lightVector = normalize( lightLocation - position.xyz );
			const vec3 reflectedVector = normalize( reflect( lightVector, normal.xyz ) );

			// lighting calculation
			const float lightDot = dot( normal.xyz, lightVector );
			const float dLight = distance( position.xyz, lightLocation );

			const float distanceFactor = min( 1.0f / ( pow( dLight, 2.0f ) ), maxDistFactor );
			const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
			const float specularContribution = distanceFactor * pow( max( dot( -reflectedVector, viewVector ), 0.0f ), roughness );

			lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
		}

		// vec3 outputValue = ( 1.0f - depth.r ) * color.rgb;
		// vec3 outputValue *= normal.xyz;
		// vec3 outputValue = abs( position.rgb );
		// vec3 outputValue = 0.5f * ( normal.rgb + vec3( 1.0f ) );

		vec3 outputValue = lightContribution * color.rgb;

		// switch ( writeLoc.x % 5 ) {
		// case 0: outputValue = lightContribution; 					break;
		// case 1: outputValue = 0.5f * ( normal.rgb + vec3( 1.0f ) );	break;
		// case 2: outputValue = abs( position.rgb );					break;
		// case 3: outputValue = vec3( 1.0f ) - depth.rrr;				break;
		// case 4: outputValue = lightContribution;						break;
		// default: break;
		// }

		// int fcxmod2 = int( writeLoc.x ) % 2;
		// int fcymod3 = int( writeLoc.y ) % 3;
		// if ( ( fcymod3 == 0 ) || ( fcxmod2 == 0 ) ) {
		// 	outputValue = color.rgb;
		// }

		// imageStore( accumulatorTexture, writeLoc, vec4( outputValue + ( blueNoise / 511.0f ), 1.0f ) );

		vec4 previous = imageLoad( accumulatorTexture, writeLoc );
		outputValue = mix( previous.xyz, outputValue.xyz, 0.01f );
		imageStore( accumulatorTexture, writeLoc, vec4( outputValue, 1.0f ) );

	} else {
		imageStore( accumulatorTexture, writeLoc, vec4( vec3( 0.0f ), 1.0f ) );

		// streaks in the negative space, interesting
		// vec4 previous = imageLoad( accumulatorTexture, writeLoc - ivec2( 1, 2 ) );
		// imageStore( accumulatorTexture, writeLoc, vec4( previous.rgb, 1.0f ) );
	}

}