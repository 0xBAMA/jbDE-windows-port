
// tonemapping stuff
// APPROX
// --------------------------
vec3 CheapACES ( vec3 v ) {
	v *= 0.6f;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

// OFFICIAL
// --------------------------
mat3 ACESInputMat = mat3(
	0.59719f, 0.35458f, 0.04823f,
	0.07600f, 0.90834f, 0.01566f,
	0.02840f, 0.13383f, 0.83777f
);

mat3 ACESOutputMat = mat3(
	1.60475f, -0.53108f, -0.07367f,
	-0.10208f,  1.10813f, -0.00605f,
	-0.00327f, -0.07276f,  1.07602f
);

vec3 MatrixMultiply ( mat3 m, vec3 v ) {
	float x = m[ 0 ][ 0 ] * v[ 0 ] + m[ 0 ][ 1 ] * v[ 1 ] + m[ 0 ][ 2 ] * v[ 2 ];
	float y = m[ 1 ][ 0 ] * v[ 1 ] + m[ 1 ][ 1 ] * v[ 1 ] + m[ 1 ][ 2 ] * v[ 2 ];
	float z = m[ 2 ][ 0 ] * v[ 1 ] + m[ 2 ][ 1 ] * v[ 1 ] + m[ 2 ][ 2 ] * v[ 2 ];
	return vec3( x, y, z );
}

vec3 RTT_ODT_Fit ( vec3 v ) {
	vec3 a = v * ( v + 0.0245786f) - 0.000090537f;
	vec3 b = v * ( 0.983729f * v + 0.4329510f ) + 0.238081f;
	return a / b;
}

vec3 aces_fitted ( vec3 v ) {
	v = MatrixMultiply( ACESInputMat, v );
	v = RTT_ODT_Fit( v );
	return MatrixMultiply( ACESOutputMat, v );
}


vec3 uncharted2(vec3 v) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;

	float ExposureBias = 2.0f;
	v *= ExposureBias;

	return ( ( ( v * ( A * v + C * B ) + D * E ) / ( v * ( A * v + B ) + D * F ) ) - E / F )
				* ( ( ( W * ( A * W + C * B ) + D * E ) / ( W * ( A * W + B ) + D * F ) ) - E / F );
}

vec3 Rienhard ( vec3 v ) {
	return v / ( vec3( 1.0 ) + v );
}

vec3 Rienhard2 ( vec3 v ) {
	const float L_white = 4.0;
	return (v * ( vec3( 1.0 ) + v / ( L_white * L_white ) ) ) / ( 1.0 + v );
}

vec3 TonemapUchimura ( vec3 v ) {
	const float P = 1.0;  // max display brightness
	const float a = 1.0;  // contrast
	const float m = 0.22; // linear section start
	const float l = 0.4;  // linear section length
	const float c = 1.33; // black
	const float b = 0.0;  // pedestal

	// Uchimura 2017, "HDR theory and practice"
	// Math: https://www.desmos.com/calculator/gslcdxvipg
	// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
	float l0 = ( ( P - m ) * l ) / a;
	float L0 = m - m / a;
	float L1 = m + ( 1.0 - m ) / a;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = ( a * P ) / ( P - S1 );
	float CP = -C2 / P;

	vec3 w0 = 1.0 - smoothstep( 0.0, m, v );
	vec3 w2 = step( m + l0, v );
	vec3 w1 = 1.0 - w0 - w2;

	vec3 T = m * pow( v / m, vec3( c ) ) + vec3( b );
	vec3 S = P - ( P - S1 ) * exp( CP * ( v - S0 ) );
	vec3 L = m + a * ( v - vec3( m ) );

	return T * w0 + L * w1 + S * w2;
}

vec3 TonemapUchimura2 ( vec3 v ) {
	const float P = 1.0;  // max display brightness
	const float a = 1.7;  // contrast
	const float m = 0.1; // linear section start
	const float l = 0.0;  // linear section length
	const float c = 1.33; // black
	const float b = 0.0;  // pedestal

	float l0 = ( ( P - m ) * l ) / a;
	float L0 = m - m / a;
	float L1 = m + ( 1.0 - m ) / a;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = ( a * P ) / ( P - S1 );
	float CP = -C2 / P;

	vec3 w0 = 1.0 - smoothstep( 0.0, m, v );
	vec3 w2 = step( m + l0, v );
	vec3 w1 = 1.0 - w0 - w2;

	vec3 T = m * pow( v / m, vec3( c ) ) + vec3( b );
	vec3 S = P - ( P - S1 ) * exp( CP * ( v - S0 ) );
	vec3 L = m + a * ( v - vec3( m ) );

	return T * w0 + L * w1 + S * w2;
}

vec3 tonemapUnreal3 ( vec3 v ) {
	return v / ( v + 0.155 ) * 1.019;
}


#define toLum(color) dot(color, vec3(0.2125, 0.7154, 0.0721) )
#define lightAjust(a,b) ((1.0-b)*(pow(1.0-a,vec3(b+1.0))-1.0)+a)/b
#define reinhard(c,l) c*(l/(1.0+l)/l)
vec3 JTtonemap ( vec3 x ) {
	float l = toLum( x );
	x = reinhard( x, l );
	float m = max( x.r, max( x.g, x.b ) );
	return min( lightAjust( x / m, m ), x );
}
#undef toLum
#undef lightAjust
#undef reinhard


vec3 robobo1221sTonemap ( vec3 x ) {
	return sqrt( x / ( x + 1.0f / x ) ) - abs( x ) + x;
}

vec3 roboTonemap ( vec3 c ) {
	return c / sqrt( 1.0 + c * c );
}

vec3 jodieRoboTonemap ( vec3 c ) {
	float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	vec3 tc = c / sqrt( c * c + 1.0 );
	return mix( c / sqrt( l * l + 1.0 ), tc, tc );
}

vec3 jodieRobo2ElectricBoogaloo ( const vec3 color ) {
	float luma = dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
	// tonemap curve goes on this line
	// (I used robo here)
	vec4 rgbl = vec4( color, luma ) * inversesqrt( luma * luma + 1.0 );
	vec3 mappedColor = rgbl.rgb;
	float mappedLuma = rgbl.a;
	float channelMax = max( max( max( mappedColor.r, mappedColor.g ), mappedColor.b ), 1.0 );

	// this is just the simplified/optimised math
	// of the more human readable version below
	return ( ( mappedLuma * mappedColor - mappedColor ) - ( channelMax * mappedLuma - mappedLuma ) ) / ( mappedLuma - channelMax );

	const vec3 white = vec3( 1.0 );

	// prevent clipping
	vec3 clampedColor = mappedColor / channelMax;

	// x is how much white needs to be mixed with
	// clampedColor so that its luma equals the
	// mapped luma
	//
	// mix(mappedLuma/channelMax,1.,x) = mappedLuma;
	//
	// mix is defined as
	// x*(1-a)+y*a
	// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/mix.xhtml
	//
	// (mappedLuma/channelMax)*(1.-x)+1.*x = mappedLuma

	float x = ( mappedLuma - mappedLuma * channelMax ) / ( mappedLuma - channelMax );
	return mix( clampedColor, white, x );
}

vec3 jodieReinhardTonemap ( vec3 c ) {
	float l = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	vec3 tc = c / ( c + 1.0 );
	return mix( c /( l + 1.0 ), tc, tc );
}

vec3 jodieReinhard2ElectricBoogaloo(const vec3 color){
	float luma = dot(color, vec3(.2126, .7152, .0722));

	// tonemap curve goes on this line
	// (I used reinhard here)
	vec4 rgbl = vec4(color, luma) / (luma + 1.);

	vec3 mappedColor = rgbl.rgb;
	float mappedLuma = rgbl.a;
	float channelMax = max( max( max( mappedColor.r, mappedColor.g ), mappedColor.b ), 1.0 );

	// this is just the simplified/optimised math
	// of the more human readable version below
	return ( ( mappedLuma * mappedColor - mappedColor ) - ( channelMax * mappedLuma - mappedLuma ) ) / ( mappedLuma - channelMax );

	const vec3 white = vec3( 1.0 );

	// prevent clipping
	vec3 clampedColor = mappedColor / channelMax;

	// x is how much white needs to be mixed with
	// clampedColor so that its luma equals the
	// mapped luma
	//
	// mix(mappedLuma/channelMax,1.,x) = mappedLuma;
	//
	// mix is defined as
	// x*(1-a)+y*a
	// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/mix.xhtml
	//
	// (mappedLuma/channelMax)*(1.-x)+1.*x = mappedLuma

	float x = ( mappedLuma - mappedLuma * channelMax ) / ( mappedLuma - channelMax );
	return mix( clampedColor, white, x );
}

#define AGX_LOOK 2

// AgX
// ->

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
 
#if AGX_LOOK == 1
  // Golden
  slope = vec3(1.0, 0.9, 0.5);
  power = vec3(0.8);
  sat = 0.8;
#elif AGX_LOOK == 2
  // Punchy
  slope = vec3(1.0);
  power = vec3(1.35, 1.35, 1.35);
  sat = 1.4;
#endif
  
  // ASC CDL
  val = pow(val * slope + offset, power);
  return luma + sat * (val - luma);
}

// <-

vec3 do_agx( vec3 fragColor)
{
  vec3 col = fragColor.xyz;
  
  // AgX
  // ->
  col = agx(col);
  col = agxLook(col);
  col = agxEotf(col);
  // <-
  
  return col;
}

// https://www.shadertoy.com/view/X3GGRz justjohn
vec3 tonemapLMS(vec3 sRGB) {
	// NOTE: Tune these params based on your use case.
	// Desmos graph: https://www.desmos.com/calculator/cvt2brlyl3
	const float EXPOSURE = 1.5;
	const float CONTRAST = 1.5;
	const float RANGE = 1.5;

	const mat3 sRGB_to_LMS = transpose(mat3(
		0.31399022, 0.63951294, 0.04649755,
		0.15537241, 0.75789446, 0.08670142,
		0.01775239, 0.10944209, 0.87256922));

	const mat3 LMS_to_sRGB = transpose(mat3(
		5.47221206, -4.6419601 ,  0.16963708,
		-1.1252419 ,  2.29317094, -0.1678952 ,
		0.02980165, -0.19318073,  1.16364789));
		
	const vec3 sRGB_to_Y = vec3(0.2126729, 0.7151522, 0.0721750);

	// Apply tonescale in LMS

	vec3 LMS = sRGB_to_LMS * sRGB;

	LMS = pow(EXPOSURE * LMS, vec3(CONTRAST / RANGE));
	LMS = LMS / (LMS + 1.0);
	LMS = pow(LMS, vec3(RANGE));

	sRGB = LMS_to_sRGB * LMS;

	// Apply gamut mapping in sRGB

	float Y = dot(sRGB_to_Y, sRGB);
	if (Y > 1.0)
		return vec3(1.0);
		
	float minimum = min(sRGB.r, min(sRGB.g, sRGB.b));
	if (minimum < 0.0)
		sRGB = mix(sRGB, vec3(Y), -minimum / (Y - minimum));

	float maximum = max(sRGB.r, max(sRGB.g, sRGB.b));
	if (maximum > 1.0)
		sRGB = mix(sRGB, vec3(Y), (1.0 - maximum) / (Y - maximum));

	return sRGB;
}

vec3 gamma_correct( vec3 linear ) {
	bvec3 cutoff = lessThan( linear, vec3( 0.0031308f ) );
	vec3 higher = 1.055f * pow( linear, vec3( 1.0f / 2.4f ) ) - 0.055f;
	vec3 lower = linear * 12.92f;
	return mix( higher, lower, cutoff );
}

vec3 LMSTonemap( in vec3 c ) {
	return gamma_correct( tonemapLMS( c ) );
}


vec3 Tonemap( int tonemapMode, vec3 col ) {
	switch ( tonemapMode ) {
			case 0: // None (Linear)
					break;
			case 1: // ACES (Narkowicz 2015)
					col.xyz = CheapACES( col.xyz );
					break;
			case 2: // Unreal Engine 3
					col.xyz = pow( tonemapUnreal3( col.xyz ), vec3( 2.8 ) );
					break;
			case 3: // Unreal Engine 4
					col.xyz = aces_fitted( col.xyz );
					break;
			case 4: // Uncharted 2
					col.xyz = uncharted2( col.xyz );
					break;
			case 5: // Gran Turismo
					col.xyz = TonemapUchimura( col.xyz );
					break;
			case 6: // Modified Gran Turismo
					col.xyz = TonemapUchimura2( col.xyz );
					break;
			case 7: // Rienhard
					col.xyz = Rienhard( col.xyz );
					break;
			case 8: // Modified Rienhard
					col.xyz = Rienhard2( col.xyz );
					break;
			case 9: // jt_tonemap
					col.xyz = JTtonemap( col.xyz );
					break;
			case 10: // robobo1221s
					col.xyz = robobo1221sTonemap( col.xyz );
					break;
			case 11: // robo
					col.xyz = roboTonemap( col.xyz );
					break;
			case 12: // jodieRobo
					col.xyz = jodieRoboTonemap( col.xyz );
					break;
			case 13: // jodieRobo2
					col.xyz = jodieRobo2ElectricBoogaloo( col.xyz );
					break;
			case 14: // jodieReinhard
					col.xyz = jodieReinhardTonemap( col.xyz );
					break;
			case 15: // jodieReinhard2
					col.xyz = jodieReinhard2ElectricBoogaloo( col.xyz );
					break;
			case 16: // AgX1 from https://www.shadertoy.com/view/ctffRr
					col.xyz = do_agx( col.xyz );
					break;
			case 17: // 
					col.xyz = LMSTonemap( col.xyz );
					break;

			// some more agx variants
			// https://www.shadertoy.com/view/clGGWG agx mmts
			// https://www.shadertoy.com/view/Dt3XDr agx ds
	}
	return col;
}

vec3 GammaCorrect ( float gammaValue, vec3 col ) {
	return pow( col, vec3( 1.0 / gammaValue ) );
}
