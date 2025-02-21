#version 450

in vec4 color;
in vec3 position;
in vec3 normal;

out vec4 fragColor;

uniform int colorMode;
uniform vec3 lightPos;



void main() {
  fragColor = color;

  if ( colorMode == 0 ) { // color handling for points
    float distanceToCenter = distance( gl_PointCoord.xy, vec2( 0.5, 0.5 ) );
    if ( distanceToCenter >= 0.5 ) discard;
  //   if ( distanceToCenter >= 0.4 ) fragColor = vec4( vec3( 0.0 ), color.a );
  }

  // float scalefactor = mix( smoothstep( 1.15, 0.0, gl_FragCoord.z ), 1.25 - gl_FragCoord.z, 0.8);
  // float scalefactor = smoothstep( 1.0, 0.25, gl_FragCoord.z / 0.3);
  float scalefactor = 1.25 - gl_FragCoord.z / 3.;
  fragColor.xyz *= scalefactor;

  if ( colorMode == 3 ) {
    if ( !gl_FrontFacing ) {

      // doing lighting here
      vec3 viewerPos = vec3( 0.0, 0.0, -1.0 );

      // phong parameters
      vec3 l = normalize( lightPos - position );
      vec3 v = normalize( viewerPos - position );
      vec3 n = normalize( normal );
      vec3 r = normalize( reflect( l, n ) );

      float d = max( dot( n, l ), 0.0 );
      float s = pow( max( dot( r, v ), 0.0 ), 10. );

      fragColor.xyz += d * vec3( 0.6, 0.0, 0.0 );
      if ( dot( n, l ) > 0.0 )
        fragColor.xyz += s * vec3( 0.0, 0.0, 0.5 );

      // if ( gl_FragCoord.x > ( 2560 / 2 ) )
        // fragColor.xyz = vec3( s, d, 0.265 / distance( lightPos, position ) );
        // fragColor.xyz = vec3( 0.265 / distance( lightPos, position ) );

      if ( int( gl_FragCoord.y ) % 3 == 0 )
        fragColor.xyz *= 0.4;
    }
  }
}
