#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, r32ui ) uniform uimage2D lineIntermediateBuffer;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
//=============================================================================================================================

#include "srgbConvertMini.h"

uniform vec3 color;

void main () {
    uint lineValue = imageLoad( lineIntermediateBuffer, ivec2( gl_GlobalInvocationID.xy ) ).r;
    vec4 accumValue = imageLoad( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ) );

    // blending the line value over the accumulator (color from uniform value, alpha from intermediate buffer)
    if ( lineValue != 0 ) {
        // blend with the accumulator contents...
        vec4 previousColor = accumValue;
        previousColor.rgb = rgb_to_srgb( previousColor.rgb );

        // alpha blending, new sample over running color
        float alphaSquared = pow( clamp( lineValue / 255.0f, 0.0f, 1.0f ), 2.0f );
        previousColor.a = max( alphaSquared + previousColor.a * ( 1.0f - alphaSquared ), 0.001f );
        previousColor.rgb = color.rgb * alphaSquared + previousColor.rgb * previousColor.a * ( 1.0f - alphaSquared );
        previousColor.rgb /= previousColor.a;

        vec3 outputCol = srgb_to_rgb( previousColor.rgb );
        imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( outputCol, 1.0f ) );
    }
}
