// these six noise functions are from https://www.shadertoy.com/view/llVGzG
// uniform noise
vec3 getUniformNoise (){
	return vec3( fract( sin( dot( gl_GlobalInvocationID.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 ) );
}

// interleaved gradient noise
vec3 getInterleavedGradientNoise (){
	// Jimenez 2014, "Next Generation Post-Processing in Call of Duty"
	float f = 0.06711056 * float( gl_GlobalInvocationID.x ) + 0.00583715 * float( gl_GlobalInvocationID.y );
	return vec3( fract( 52.9829189 * fract( f ) ) );
}

// vlachos
vec3 getVlachosNoise (){
	vec3 noise = vec3( dot( vec2( 171.0, 231.0 ), gl_GlobalInvocationID.xy ) );
	return fract( noise / vec3( 103.0, 71.0, 97.0 ) );
}

// triangle helper functions
float triangleNoise ( const vec2 n ) {
	// triangle noise, in [-0.5..1.5[ range
	vec2 p = fract( n * vec2( 5.3987, 5.4421 ) );
	p += dot( p.yx, p.xy + vec2( 21.5351, 14.3137 ) );
	float xy = p.x * p.y;
	// compute in [0..2[ and remap to [-1.0..1.0[
	float noise = ( fract( xy * 95.4307 ) + fract( xy * 75.04961 ) - 1.0 );
	//noise = sign(noise) * (1.0 - sqrt(1.0 - abs(noise)));
	return noise;
}

float triangleRemap ( float n ) {
	float origin = n * 2.0 - 1.0;
	float v = origin / sqrt( abs( origin ) );
	v = max( -1.0, v );
	v -= sign( origin );
	return v;
}

vec3 triangleRemap ( const vec3 n ) {
	return vec3(
		triangleRemap( n.x ),
		triangleRemap( n.y ),
		triangleRemap( n.z )
	);
}

// vlachos triangle distribution
vec3 getVlachosTriangle () {
	// Vlachos 2016, "Advanced VR Rendering"
	vec3 noise = vec3( dot( vec2( 171.0, 231.0 ), gl_GlobalInvocationID.xy ) );
	noise = fract( noise / vec3( 103.0, 71.0, 97.0 ) );
	return triangleRemap( noise );
}

// triangle noise monochrome
vec3 getMonoTriangle () {
	// Gjøl 2016, "Banding in Games: A Noisy Rant"
	return vec3( triangleNoise( vec2( gl_GlobalInvocationID.xy ) / vec2( gl_WorkGroupSize.xy ) ) );
}

// triangle noise RGB
vec3 getRGBTriangle () {
	return vec3(
		triangleNoise( vec2( gl_GlobalInvocationID.xy ) / vec2(gl_WorkGroupSize.xy ) ),
		triangleNoise( vec2( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.1337 ) ) / vec2( gl_WorkGroupSize.xy ) ),
		triangleNoise( vec2( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.3141 ) ) / vec2( gl_WorkGroupSize.xy ) )
	);
}

const float c_goldenRatioConjugate = 0.61803398875;

float cycle ( float inputValue, int frame ) {
	return fract( inputValue + float( frame % 256 ) * c_goldenRatioConjugate );
}

vec3 cycle ( vec3 inputValue, int frame ) {
	return vec3(
		fract( inputValue.x + float( frame % 256 ) * c_goldenRatioConjugate ),
		fract( inputValue.y + float( ( frame + 1 ) % 256 ) * c_goldenRatioConjugate ),
		fract( inputValue.z + float( ( frame + 2 ) % 256 ) * c_goldenRatioConjugate )
	);
}
