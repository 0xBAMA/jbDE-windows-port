vec3 mul3( in mat3 m, in vec3 v ){return vec3(dot(v,m[0]),dot(v,m[1]),dot(v,m[2]));}
vec3 mul3( in vec3 v, in mat3 m ){return mul3(m,v);}
vec3 srgb2oklab(vec3 c) {
    mat3 m1 = mat3(
        0.4122214708,0.5363325363,0.0514459929,
        0.2119034982,0.6806995451,0.1073969566,
        0.0883024619,0.2817188376,0.6299787005
    );
    vec3 lms = mul3(m1,c);
    lms = pow(lms,vec3(1./3.));
    mat3 m2 = mat3(
        +0.2104542553,+0.7936177850,-0.0040720468,
        +1.9779984951,-2.4285922050,+0.4505937099,
        +0.0259040371,+0.7827717662,-0.8086757660
    );
    return mul3(m2,lms);
}

vec3 oklab2srgb(vec3 c) {
    mat3 m1 = mat3(
        1.0000000000,+0.3963377774,+0.2158037573,
        1.0000000000,-0.1055613458,-0.0638541728,
        1.0000000000,-0.0894841775,-1.2914855480
    );
    vec3 lms = mul3(m1,c);
    lms = lms * lms * lms;
    mat3 m2 = mat3(
        +4.0767416621,-3.3077115913,+0.2309699292,
        -1.2684380046,+2.6097574011,-0.3413193965,
        -0.0041960863,-0.7034186147,+1.7076147010
    );
    return mul3(m2,lms);
}

vec3 lab2lch( in vec3 c ){return vec3(c.x,sqrt((c.y*c.y) + (c.z * c.z)),atan(c.z,c.y));}
vec3 lch2lab( in vec3 c ){return vec3(c.x,c.y*cos(c.z),c.y*sin(c.z));}
vec3 srgb_to_oklch( in vec3 c ) { return lab2lch(srgb2oklab(c)); }
vec3 oklch_to_srgb( in vec3 c ) { return oklab2srgb(lch2lab(c)); }