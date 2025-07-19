#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, r32f ) uniform image2D fieldMax;	// the max image, being written to

// the field buffer image
uniform isampler2D bufferImageY;
uniform isampler2D bufferImageCount;

void main () {
    // pixel location
    const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
    const ivec2 iS = ivec2( textureSize( bufferImageY, 0 ).xy );

    // bounds check
    if ( loc.x < iS.x && loc.y < iS.y ) {
        // find the brightness of the pixel... you can get this from the XYZ constant's Y value
        const float brightness = float( texture( bufferImageY, vec2( loc + 0.5f ) / iS ).r ) / 1024.0f;

        // and store the result for layer N + 1
        imageStore( fieldMax, loc, vec4( brightness ) );
    }
}