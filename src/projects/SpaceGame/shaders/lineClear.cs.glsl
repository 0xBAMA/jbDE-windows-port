#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, r32ui ) uniform uimage2D lineIntermediateBuffer;
//=============================================================================================================================
void main () {
    imageStore( lineIntermediateBuffer, ivec2( gl_GlobalInvocationID.xy ), uvec4( 0 ) );
}
