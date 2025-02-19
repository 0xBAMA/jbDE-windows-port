// from https://www.shadertoy.com/view/7lGXWK
#define PI 3.1415926

// ================ RANDOM FUNCTIONS START ================

// unit circle
vec2 randCircle(float rand1, float rand2) {
    float u = 2.0*PI*rand1;  // θ
    float v = sqrt(rand2);  // r
    return v*vec2(cos(u), sin(u));
}

// sector with abs(θ)<angle
vec2 randSector(float angle, float rand1, float rand2) {
    float u = angle*(2.0*rand1-1.0);  // θ
    float v = sqrt(rand2);  // r
    return v*vec2(cos(u), sin(u));
}

// ellipse with major and minor radius
vec2 randEllipse(float rx, float ry, float rand1, float rand2) {
    float u = 2.0*PI*rand1;  // θ
    float v = sqrt(rand2);  // r
    vec2 circ = v * vec2(cos(u), sin(u));  // unit circle
    return vec2(rx, ry) * circ;  // linear transform
}

// x²-|x|y+y² < 1
vec2 randHeart(float rand1, float rand2) {
    float u = 2.0*PI*rand1;  // θ
    float v = sqrt(rand2);  // r
    vec2 c = v*vec2(cos(u), sin(u));  // unit circle
    c = mat2(1.0,1.0,-0.577,0.577)*c;  // ellipse
    if (c.x<0.0) c.y=-c.y;  // mirror
    return c;
}

// -1 <= x,y < 1
vec2 randSquare(float rand1, float rand2) {
    float u = 2.0*rand1-1.0;
    float v = 2.0*rand2-1.0;
    return vec2(u, v);
}

// convex quadrilateral defined by ccw vertices
vec2 randQuad(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float rand1, float rand2) {
    float u = rand1;
    float v = rand2;
    p1 -= p0, p2 -= p0, p3 -= p0;  // one point and three vectors
    float area1 = 0.5*(p1.x*p2.y-p1.y*p2.x);  // area of the first triangle
    float area2 = 0.5*(p2.x*p3.y-p2.y*p3.x);  // area of the second triangle
    float th = area1 / (area1+area2);  // threshold to decide which triangle
    if (u > th) p1 = p2, p2 = p3, u = (u-th)/(1.0-th);  // use the second triangle
    else u /= th;  // use the first triangle
    return p0 + mix(p1, p2, u) * sqrt(v);  // sample inside triangle
    if (u+v>1.) u=1.-u, v=1.-v; return p0 + p1*u + p2*v;  // avoid square root, may or may not be faster
}

// regular n-gon with radius 1
vec2 randPolygon(float n, float rand1, float rand2) {
    float u = n*rand1;
    float v = rand2;
    float ui = floor(u);  // index of triangle
    float uf = fract(u);  // interpolating in triangle
    vec2 v0 = vec2(cos(2.*PI*ui/n), sin(2.*PI*ui/n));  // triangle edge #1
    vec2 v1 = vec2(cos(2.*PI*(ui+1.)/n), sin(2.*PI*(ui+1.)/n));  // triangle edge #2
    return sqrt(v) * mix(v0, v1, uf);  // sample inside triangle
}

// regular n-star with normalized size
vec2 randStar(float n, float rand1, float rand2) {
    float u = n*rand1;
    float v = rand2;
    float ui = floor(u);  // index of triangle
    float uf = fract(u);  // interpolating in rhombus
    vec2 v0 = vec2(cos(2.*PI*ui/n), sin(2.*PI*ui/n));  // rhombus edge #1
    vec2 v1 = vec2(cos(2.*PI*(ui+1.)/n), sin(2.*PI*(ui+1.)/n));  // rhombus edge #2
    vec2 p = v0 * v + v1 * uf;  // sample rhombus
    return p / (n*sin(2.*PI/n)/PI);  // normalize size
}

// ring formed by two concentric circles with radius r0 and r1
vec2 randConcentric(float r0, float r1, float rand1, float rand2) {
    float u = 2.0*PI*rand1;  // θ
    float v = sqrt(mix(r0*r0, r1*r1, rand2));  // r
    return v * vec2(cos(u), sin(u));  // polar to Cartesian
}

// intersection of two circles with centers (0,±c) and radius r
vec2 randIntersection(float c, float r, float rand1, float rand2) {
    // https://www.desmos.com/calculator/sctxdxh1td
    float u = rand1;
    float v = 2.0*rand2-1.0;
    float x1 = sqrt(r*r-c*c);  // x range [-x1, x1]
    float i1 = 0.5*r*r*asin(x1/r)+0.5*x1*sqrt(r*r-x1*x1)-c*x1;  // area under the curve from 0 t0 x1
    u = 2.0*i1*u - i1;  // u = Integral[0,x1,sqrt(x^2+y^2-c)]
    float x = 0.0;  // solve for the x-coordinate
    for (int iter=0; iter<6; iter++) {  // Newton-Raphson
        float cdf = 0.5*(r*r*asin(x/r)+x*sqrt(r*r-x*x))-c*x;
        float pdf = sqrt(r*r-x*x)-c;
        x -= (cdf-u)/pdf;
    }
    float y = (sqrt(r*r-x*x)-c) * v;  // y-coordinate
    return vec2(x, y);
}

// union of two circles with centers (±c,0) and radius r
vec2 randUnion(float c, float r, float rand1, float rand2) {
    // https://www.desmos.com/calculator/ddyum0rkgw
    float u = 2.0*rand1-1.0;
    float v = 2.0*rand2-1.0;
    float s = sign(u);
    float x1 = r+c;  // (x1,0)
    float h0 = sqrt(r*r-c*c);  // (0,h0)
    float i0 = -0.5*(c*h0+r*r*atan(c/h0));  // defaultIntegral(0.0)
    float i1 = 0.25*PI*r*r;  // defaultIntegral(1.0)
    u = mix(i0, i1, abs(u));  // x = defaultIntegral(u)
    float x = c;  // iteration starts at a point of inflection
    for (int iter=0; iter<6; iter++) {  // Newton-Raphson, solve for x
        float pdf = sqrt(r*r-(x-c)*(x-c));
        float cdf = 0.5*((x-c)*pdf+r*r*atan((x-c)/pdf));
        x -= (cdf-u)/pdf;
    }
    float y = sqrt(r*r-(x-c)*(x-c)) * v;  // y-coordinate
    return vec2(s*x, y);
}

// subtraction of a circle with center (c,0) from a circle with (0,0)
vec2 randSubtraction(float c, float r, float rand1, float rand2) {
    // https://www.desmos.com/calculator/jk1okdxoks
    float u = rand1;
    float v = 2.0*rand2-1.0;
    float x1 = 0.5*c;  // x in [-r, x1]
    float y1 = sqrt(r*r-0.25*c*c);  // rightmost (x1,±y1)
    float a1 = y1*c;  // area of the middle part
    float i1 = 0.5*(x1*sqrt(r*r-x1*x1)+r*r*asin(x1/r))-x1*y1;
    float a2 = 2.0*i1;  // area of the top/bottom part
    float th = a1/(a1+a2);  // decide the point lie in which part
    if (u < th) {
        float y = y1*v;  // y-coordinate
        float x = c*(u/th) - sqrt(r*r-y*y);  // x-coordinate, shear a rectangle
        return vec2(x, y);
    }
    u = i1 * (2.*((u-th)/(1.-th)) - 1.);  // x = defaultIntegral(u)
    float x = 0.0;  // solve for x-coordinate
    for (int iter=0; iter<6; iter++) {  // Newton-Raphson
        float cdf = 0.5*(x*sqrt(r*r-x*x)+r*r*asin(x/r))-y1*x;
        float pdf = sqrt(r*r-x*x)-y1;
        x -= (cdf-u)/pdf;
    }
    float y = sign(v) * (y1+(sqrt(r*r-x*x)-y1)*abs(v));  // y-coordinate, note that v in [-1, 1)
    return vec2(x, y);
}

// |y| < cos(x)-k
vec2 randCosine(float k, float rand1, float rand2) {
    float u = 2.0*rand1-1.0;  // related to x
    float v = 2.0*rand2-1.0;  // related to y
    float x1 = acos(k);  // maximum x
    float y1 = sin(x1) - k*x1;  // a quarter of area
    float yt = u*y1;  // randomly chosen area position
    float x = 0.0;  // start iteration
    for (int iter=0; iter<6; iter++) {  // Newton-Raphson
        float cdf = sin(x)-k*x;
        float pdf = cos(x)-k;
        x -= (cdf-yt)/pdf;
    }
    float y = cos(x)-k;  // calculate y from x
    return vec2(x, y*v);  // apply random to y
}

// r(θ) = 1-cos(θ)
vec2 randCardioid(float rand1, float rand2) {
    // integrate (1-cos(θ))², find the inverse on [0,2π)
    float u = 3.0*PI*rand1;  // integral is 3π
    float v = sqrt(rand2);  // r
    float theta = PI;  // iteration starting point
    for (int iter=0; iter<10; iter++) {
        float cdf = 0.25*(sin(2.0*theta)-8.0*sin(theta)+6.0*theta);  // integral
        float pdf = (1.0-cos(theta))*(1.0-cos(theta));  // area element
        theta -= (cdf-u)/pdf;  // Newton-Raphson
    }
    float r = 1.0-cos(theta);  // polar equation
    return v * r * vec2(cos(theta), sin(theta));  // polar coordinate
}

// r(θ) = cos(nθ)
vec2 randRose(float n, float rand1, float rand2) {
    // integrate cos(nθ)², split into n intervals and inverse in [-π/2n,π/2n)
    float u = PI*rand1;  // integral in [0,2π) is π
    float v = sqrt(rand2);  // r
    float ui = PI/n*floor(2.0*n/PI*u);  // center of each "petal"
    float uf = mod(u,PI/(2.0*n))-PI/(4.0*n);  // parameter of each "petal", integralOfCosNThetaSquared(θ)
    float theta = 0.0;  // iteration starting point
    for (int iter=0; iter<9; iter++) {
        float cdf = 0.5*theta + sin(2.0*n*theta)/(4.0*n);  // integral of cos(nθ)²
        float pdf = 0.5 + 0.5*cos(2.0*n*theta);  // cos(nθ)²
        theta -= (cdf-uf)/pdf;  // Newton-Raphson
    }
    theta = ui + theta;  // move to petal
    float r = cos(n*theta);  // polar equation
    return v * r * vec2(cos(theta), sin(theta));  // polar coordinate
}

// logarithmic spiral, solvable analytically
vec2 randSpiral(float k, float a0, float a1, float rand1, float rand2) {
    float s0 = exp(2.0*PI*k*a0), s1 = exp(2.0*PI*k*a1);  // r = s*exp(kθ)
    // (x,y)=s*exp(kθ)*(cos(θ),sin(θ)), ∂(x,y)/∂(s,θ)=s*exp(2kθ), s*exp(2kθ)*∂(s,θ)/∂(u,v)=(s1²-s0²)/4k
    // ∂u∂v=(2s/(s1²-s0²))∂s*(2kexp(2kθ))∂θ, integrate and invert to find θ(u) and s(v)
    float u = rand1;  // for angle
    float v = rand2;  // for distance
    float theta = log(1.0-u)/(2.0*k);  // θ(u)
    float s = sqrt(mix(s0*s0, s1*s1, v));  // s(v)
    return s * exp(k*theta) * vec2(cos(theta),sin(theta));  // formula
}

// ================ RANDOM FUNCTIONS END. ================



// scene
vec3 intersectRay(vec3 ro, vec3 rd) {
    vec3 col = mix(vec3(0.0), 0.1*vec3(0.2,0.3,0.8), 0.5+0.5*sin(4.0*rd.x)*cos(4.0*rd.y)*sin(4.0*rd.z));
    vec3 p;
    // red
    p = ro+rd*(-(ro.y+4.0)/rd.y);
    if (length(p.xz-vec2(-0.8,-0.8))<0.2) col+=20.0*vec3(1.0,0.3,0.1);
    // yellow
    p = ro+rd*(-(ro.y+10.0)/rd.y);
    if (length(p.xz-vec2(3.0,2.0))<0.2) col+=20.0*vec3(1.0,0.8,0.5);
    // blue
    p = ro+rd*(-(ro.y+20.0)/rd.y);
    if (length(p.xz-vec2(0.0,-1.5))<0.2) col+=20.0*vec3(0.1,0.6,1.0);
    // green
    p = ro+rd*(-(ro.y+50.0)/rd.y);
    if (length(p.xz-vec2(-12.0,1.0))<0.2) col+=40.0*vec3(0.1,1.0,0.4);
    return col;
}

// quasi-random
float vanDerCorput(float n, float b) {
    float x = 0.0;
    float e = 1.0 / b;
    while (n > 0.0) {
        float d = mod(n, b);
        x += d * e;
        e /= b;
        n = floor(n / b);
    }
    return x;
}

// main
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // random number seed
    vec3 p3 = fract(fragCoord/iResolution.xy*.1031).xyx;
    p3 += dot(p3, p3.zyx + 31.32);
    float h = fract((p3.x + p3.y) * p3.z);
    float seed = floor(65536.*h) + float(iFrame);

    // camera parameters
    const vec3 POS = vec3(3.0,3.0,0.0);
    const float SCALE = 1.5;  // larger = smaller (more view field)
    const float DIST = 0.2;  // larger = smaller
    const float VIEW_FIELD = 0.5;  // larger = larger + more perspective
    const float APERTURE = 0.02;  // larger = blurred

    // sample aperture shape
    float rand1 = vanDerCorput(seed, 2.);
    float rand2 = vanDerCorput(seed, 3.);
    vec2 rnd;

    ivec2 shape_id = ivec2(floor(4.0*fragCoord.xy/iResolution.xy));
    if (shape_id.y==3) {
        if (shape_id.x==0) rnd = randCircle(rand1, rand2);
        if (shape_id.x==1) rnd = randSector(0.8*PI, rand1, rand2);
        if (shape_id.x==2) rnd = randEllipse(1.2, 0.8, rand1, rand2);
        if (shape_id.x==3) rnd = randHeart(rand1, rand2);
    }
    if (shape_id.y==2) {
        if (shape_id.x==0) rnd = randSquare(rand1, rand2);
        if (shape_id.x==1) rnd = randQuad(vec2(-1.0,-1.0), vec2(0.8,-0.9), vec2(1.0,1.2), vec2(-0.4,0.8), rand1, rand2);
        if (shape_id.x==2) rnd = randPolygon(5., rand1, rand2);
        if (shape_id.x==3) rnd = randStar(5., rand1, rand2);
    }
    if (shape_id.y==1) {
        if (shape_id.x==0) rnd = randConcentric(0.6, 1.1, rand1, rand2);
        if (shape_id.x==1) rnd = randIntersection(0.9, 1.6, rand1, rand2);
        if (shape_id.x==2) rnd = randUnion(0.6, 0.8, rand1, rand2);
        if (shape_id.x==3) rnd = randSubtraction(1.0, 1.0, rand1, rand2);
    }
    if (shape_id.y==0) {
        if (shape_id.x==0) rnd = randCosine(0.4, rand1, rand2);
        if (shape_id.x==1) rnd = randCardioid(rand1, rand2);
        if (shape_id.x==2) rnd = randRose(5., rand1, rand2);
        if (shape_id.x==3) rnd = randSpiral(0.2, -0.1, 0.3, rand1, rand2);
    }

    // camera
    vec3 ro = POS+vec3(0,DIST,0);
    vec2 randuv = vec2(vanDerCorput(seed,5.), vanDerCorput(seed,7.));
    vec2 uv = SCALE*(2.0*fract(4.0*(fragCoord.xy+randuv-0.5)/iResolution.xy)-1.0);
    vec2 sc = iResolution.xy/length(iResolution.xy);
    vec2 offset = APERTURE*rnd;
    ro.xz += offset;
    vec3 rd = vec3(VIEW_FIELD*uv*sc+vec2(-0.25,0.0)-offset/DIST, -1.0).xzy;

    // calculate pixel color
    vec3 col = intersectRay(ro, rd);
    vec4 rgbn = texelFetch(iChannel0, ivec2(int(fragCoord.x), int(fragCoord.y)), 0);
    fragColor = vec4((rgbn.xyz*rgbn.w + col)/(rgbn.w+1.0), rgbn.w+1.0);
}
