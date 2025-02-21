#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// four images for atomic writes - red, green, blue, luma
layout( r32ui ) uniform uimage2D redImage;
layout( r32ui ) uniform uimage2D greenImage;
layout( r32ui ) uniform uimage2D blueImage;
layout( r32ui ) uniform uimage2D lumaImage;

// buffers for per-column min/max - r, g, b, luma interleaved
layout( binding = 2, std430 ) readonly buffer perColumnMins { uint columnMins[]; };
layout( binding = 3, std430 ) readonly buffer perColumnMaxs { uint columnMaxs[]; };
layout( binding = 4, std430 ) buffer binMax { uint globalMax[ 4 ]; };

// the output image
layout( rgba8 ) uniform image2D compositedResult;

// Mean error^2: 3.6705141e-06
vec3 agxDefaultContrastApprox(vec3 x) {
  vec3 x2 = x * x;
  vec3 x4 = x2 * x2;
  
  return + 15.5     * x4 * x2
         - 40.14    * x4 * x
         + 31.96    * x4
         - 6.868    * x2 * x
         + 0.4298   * x2
         + 0.1191   * x
         - 0.00232;
}

vec3 agx(vec3 val) {
  const mat3 agx_mat = mat3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992,  0.878468636469772,  0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    
  const float min_ev = -12.47393f;
  const float max_ev = 4.026069f;

  // Input transform
  val = agx_mat * val;
  
  // Log2 space encoding
  val = clamp(log2(val), min_ev, max_ev);
  val = (val - min_ev) / (max_ev - min_ev);
  
  // Apply sigmoid function approximation
  val = agxDefaultContrastApprox(val);
  return val;
}

vec3 agxEotf(vec3 val) {
  const mat3 agx_mat_inv = mat3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    
  // Undo input transform
  val = agx_mat_inv * val;
  
  // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
  //val = pow(val, vec3(2.2));

  return val;
}

vec3 agxLook(vec3 val) {
	const vec3 lw = vec3(0.2126, 0.7152, 0.0722);
	float luma = dot(val, lw);

	// Default
	vec3 offset = vec3(0.0);
	vec3 slope = vec3(1.0);
	vec3 power = vec3(1.0);
	float sat = 1.0;

	slope = vec3( 1.9f );
	power = vec3( 1.5, 1.5, 1.35 );
	sat = 1.4;

	// ASC CDL
	val = pow( val * slope + offset, power );
	return luma + sat * (val - luma);
}

vec3 do_agx( vec3 fragColor ) {
	vec3 col = fragColor.xyz;
	// AgX
	col = agx(col);
	col = agxLook(col);
	col = agxEotf(col);
	return col;
}

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	const bool drawMinMax = true;

	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	bool ref = false;

	// I would like to add some gridlines

	const uint yIndex = 255 - ( loc.y % 256 );
	switch ( loc.y / 256 ) {
		case 0: // red
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x ] || yIndex == columnMaxs[ 4 * loc.x ] ) ) {
			color.rg = vec2( 1.0f );
			ref = true;
		} else {
			color.r = float( imageLoad( redImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 0 ] );
		}
		break;

		case 1: // green
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x + 1 ] || yIndex == columnMaxs[ 4 * loc.x + 1 ] ) ) {
			color.rg = vec2( 1.0f );
			ref = true;
		} else {
			color.g = float( imageLoad( greenImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 1 ] );
		}
		break;

		case 2: // blue
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x + 2 ] || yIndex == columnMaxs[ 4 * loc.x + 2 ] ) ) {
			color.rg = vec2( 1.0f );
			ref = true;
		} else {
			color.b = float( imageLoad( blueImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 2 ] );
		}
		break;

		case 3: // luma
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x + 3 ] || yIndex == columnMaxs[ 4 * loc.x + 3 ] ) ) {
			color.rg = vec2( 1.0f );
			ref = true;
		} else {
			color.rgb = vec3( float( imageLoad( lumaImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 3 ] ) );
		}
		break;
	}

	if ( !ref ) {
		color.rgb *= 1.5f;
		color.xyz = do_agx( color.xyz );
	}

	imageStore( compositedResult, loc, color );
}