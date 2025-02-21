#version 430

in flat uint vofiIndex;
in vec3 vofiPosition;
out vec4 outColor;

flat in vec2 center;
flat in float radiusPixels;

uniform vec3 eyePosition;	// location of the viewer
uniform float numPrimitives;

#include "mathUtils.h"

// ===================================================================================================
uniform mat4 viewTransform;
uniform mat4 invViewTransform;
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 2, std430 ) buffer parametersBuffer {
	parameters_t parameters[];
};

// out float gl_FragDepth;
layout( location = 0 ) out vec4 normalResult;
layout( location = 1 ) out uvec4 primitiveID;

void main() {
	vec4 position = vec4( parameters[ vofiIndex ].data[ 1 ], parameters[ vofiIndex ].data[ 2 ], parameters[ vofiIndex ].data[ 3 ], 1.0f );
	const float radius = parameters[ vofiIndex ].data[ 4 ];

	// https://stackoverflow.com/questions/25780145/gl-pointsize-corresponding-to-world-space-size
	vec2 coord = ( gl_FragCoord.xy - center ) / radiusPixels;
	float l = length( coord );
	if ( l > 1.0f ) {
		discard;
	}
	vec3 pos = vec3( coord, sqrt( 1.0f - l * l ) ); // not quite normal - needs to be transformed

	const vec3 normal = mat3( invViewTransform ) * normalize( pos );

	// rasterizer outputs

	// writing correct depths
	const vec4 projectedPosition = viewTransform * ( position + vec4( normal * radius, 0.0f ) );
	gl_FragDepth = ( projectedPosition.z / projectedPosition.w + 1.0f ) * 0.5f;

	primitiveID = uvec4( 0, vofiIndex + 1, 0, 0 );
	normalResult = vec4( normal, 1.0f );
}