// methods for encoding and decoding normals, adapted from Aras P
// https://aras-p.info/texts/CompactNormalStorage.html#method04spheremap

// goal here is to pack a reasonable representation of the normal into the footprint of a single float
// vec3 -> 2x half floats, which are packed into a 4-byte uint using packHalf2x16()

// on the decode side, it's the same kind of thing, we want to get the normal back
// uint unpacked into half precision vec2, using unpackHalf2x16(), then restored to a vec3 normal vector

uint encode ( vec3 n ) {
    float p = sqrt( n.z * 8.0f + 8.0f );
    vec2 encoded = vec2( n.xy / p + 0.5f );

    return packHalf2x16( encoded );
}

vec3 decode ( uint enc ) {
    vec2 encoded = unpackHalf2x16( enc );

    vec2 fenc = encoded * 4.0f - 2.0f;
    float f = dot( fenc, fenc );
    float g = sqrt( 1.0f - f / 4.0f );

    vec3 n;
    n.xy = fenc * g;
    n.z = 1.0f - f / 2.0f;

    return n;
}
