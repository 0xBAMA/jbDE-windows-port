// https://www.shadertoy.com/view/ttcyRS
vec3 oklab_mix( vec3 colA, vec3 colB, float h ) {
    // https://bottosson.github.io/posts/oklab
    const mat3 kCONEtoLMS = mat3(
    0.4121656120f,  0.2118591070f,  0.0883097947f,
    0.5362752080f,  0.6807189584f,  0.2818474174f,
    0.0514575653f,  0.1074065790f,  0.6302613616f );
    const mat3 kLMStoCONE = mat3(
    4.0767245293f, -1.2681437731f, -0.0041119885f,
    -3.3072168827f, 2.6093323231f, -0.7034763098f,
    0.2307590544f, -0.3411344290f,  1.7068625689f );
    vec3 lmsA = pow( kCONEtoLMS * colA, vec3( 1.0f / 3.0f ) );
    vec3 lmsB = pow( kCONEtoLMS * colB, vec3( 1.0f / 3.0f ) );
    vec3 lms = mix( lmsA, lmsB, h );
    // gain in the middle (no oaklab anymore, but looks better?) -iq
    // lms *= 1.0f + 0.2f * h * ( 1.0f - h );
    return kLMStoCONE * ( lms * lms * lms );
}

// values from: https://physicallybased.info/
const vec3 aluminum		= vec3( 0.912f, 0.914f, 0.920f );
const vec3 aqua			= vec3( 0.020f, 0.760f, 0.870f );
const vec3 banana		= vec3( 0.634f, 0.532f, 0.111f );
const vec3 blackboard	= vec3( 0.039f, 0.039f, 0.039f );
const vec3 blood		= vec3( 0.644f, 0.003f, 0.005f );
const vec3 bone			= vec3( 0.793f, 0.793f, 0.664f );
const vec3 brass		= vec3( 0.887f, 0.789f, 0.434f );
const vec3 brick		= vec3( 0.262f, 0.095f, 0.061f );
const vec3 carrot		= vec3( 0.713f, 0.170f, 0.026f );
const vec3 charcoal		= vec3( 0.020f, 0.020f, 0.020f );
const vec3 chocolate	= vec3( 0.162f, 0.091f, 0.060f );
const vec3 chromium		= vec3( 0.638f, 0.651f, 0.663f );
const vec3 cobalt		= vec3( 0.692f, 0.703f, 0.673f );
const vec3 coffee		= vec3( 0.027f, 0.019f, 0.018f );
const vec3 concrete		= vec3( 0.510f, 0.510f, 0.510f );
const vec3 cookingOil	= vec3( 0.738f, 0.687f, 0.091f );
const vec3 copper		= vec3( 0.926f, 0.721f, 0.504f );
const vec3 eggShell		= vec3( 0.610f, 0.624f, 0.631f );
const vec3 sclera		= vec3( 0.680f, 0.490f, 0.370f );
const vec3 gasoline		= vec3( 1.000f, 0.970f, 0.617f );
const vec3 gold			= vec3( 0.944f, 0.776f, 0.373f );
const vec3 grayCard		= vec3( 0.180f, 0.180f, 0.180f );
const vec3 honey		= vec3( 0.831f, 0.397f, 0.038f );
const vec3 iron			= vec3( 0.531f, 0.512f, 0.496f );
const vec3 ketchup		= vec3( 0.164f, 0.006f, 0.002f );
const vec3 lead			= vec3( 0.632f, 0.626f, 0.641f );
const vec3 lemon		= vec3( 0.718f, 0.483f, 0.000f );
const vec3 marble		= vec3( 0.830f, 0.791f, 0.753f );
const vec3 mercury		= vec3( 0.781f, 0.779f, 0.779f );
const vec3 milk			= vec3( 0.815f, 0.813f, 0.682f );
const vec3 nickel		= vec3( 0.649f, 0.610f, 0.541f );
const vec3 nvidia		= vec3( 0.463f, 0.726f, 0.000f );
const vec3 officePaper	= vec3( 0.794f, 0.834f, 0.884f );
const vec3 pearl		= vec3( 0.800f, 0.750f, 0.700f );
const vec3 petroleum	= vec3( 0.030f, 0.027f, 0.024f );
const vec3 platinum		= vec3( 0.679f, 0.642f, 0.588f );
const vec3 sand			= vec3( 0.440f, 0.386f, 0.231f );
const vec3 sapphire		= vec3( 0.670f, 0.764f, 0.855f );
const vec3 silicon		= vec3( 0.344f, 0.367f, 0.419f );
const vec3 silver		= vec3( 0.962f, 0.949f, 0.922f );
const vec3 skinI		= vec3( 0.847f, 0.638f, 0.552f );
const vec3 skinII		= vec3( 0.799f, 0.485f, 0.347f );
const vec3 skinIII		= vec3( 0.623f, 0.433f, 0.343f );
const vec3 skinIV		= vec3( 0.436f, 0.227f, 0.131f );
const vec3 skinV		= vec3( 0.283f, 0.148f, 0.079f );
const vec3 skinVI		= vec3( 0.090f, 0.050f, 0.020f );
const vec3 snow			= vec3( 0.850f, 0.850f, 0.850f );
const vec3 tire			= vec3( 0.023f, 0.023f, 0.023f );
const vec3 titanium		= vec3( 0.616f, 0.582f, 0.544f );
const vec3 tungsten		= vec3( 0.504f, 0.498f, 0.478f );
const vec3 vanadium		= vec3( 0.520f, 0.532f, 0.541f );
const vec3 whiteboard	= vec3( 0.869f, 0.867f, 0.771f );
const vec3 zinc			= vec3( 0.802f, 0.844f, 0.863f );