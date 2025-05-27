#version 430
//=============================================================================================================================
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, r32ui ) uniform uimage2D lineIntermediateBuffer;
//=============================================================================================================================
void drawPixel ( int x, int y, float brightness ) {
    uint c = uint( 255.0f * brightness );
    // write the pixel value (max with intermediate buffer)
    imageAtomicMax( lineIntermediateBuffer, ivec2( x, y ), c );
}

// from https://zingl.github.io/bresenham.html
void bresenhamLine ( int x0, int y0, int x1, int y1 ) {
    int dx =  abs( x1 - x0 ), sx = x0 < x1 ? 1 : -1;
    int dy = -abs( y1 - y0 ), sy = y0 < y1 ? 1 : -1;
    int err = dx+dy, e2; /* error value e_xy */

    for ( ;; ) {  /* loop */
        drawPixel( x0, y0, 1.0f );
        if ( x0 == x1 && y0 == y1 ) break;
        e2 = 2 * err;
        if ( e2 >= dy ) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if ( e2 <= dx ) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }
}

void drawAALine ( int x0, int y0, int x1, int y1 ) {
    int dx = abs( x1 - x0 ), sx = x0 < x1 ? 1 : -1;
    int dy = abs( y1 - y0 ), sy = y0 < y1 ? 1 : -1;
    int err = dx - dy, e2, x2;                       /* error value e_xy */
    int ed = int( dx + dy == 0 ? 1 : sqrt( float( dx * dx ) + float( dy * dy ) ) );

    for ( ; ; ) {                                         /* pixel loop */
        drawPixel( x0, y0, 1.0f - ( 255 * abs( err - dx + dy ) / ed ) / 255.0f );
        e2 = err; x2 = x0;
        if ( 2 * e2 >= -dx ) {                                    /* x step */
            if ( x0 == x1 ) break;
            if ( e2 + dy < ed )
                drawPixel( x0, y0 + sy, 1.0f - ( 255 * ( e2 + dy ) / ed ) / 255.0f );
            err -= dy; x0 += sx;
        }
        if ( 2 * e2 <= dy ) {                                     /* y step */
            if ( y0 == y1 ) break;
            if ( dx - e2 < ed )
                drawPixel( x2 + sx,y0, 1.0f - ( 255 * ( dx - e2 ) / ed ) / 255.0f );
            err += dx; y0 += sy;
        }
    }
}
//=============================================================================================================================
uniform int offset; // the offset into the buffer, for batched lines
struct parameters_t {
    ivec2 points[ 2 ];
};
layout( binding = 0, std430 ) buffer parametersBuffer {
    parameters_t lineParameters[];
};
//=============================================================================================================================
void main () {
    // SSBO indexing
    uint myIndex = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y + offset;

    parameters_t myParameters = lineParameters[ myIndex ];
    ivec2 p1 = abs( myParameters.points[ 0 ] );
    ivec2 p2 = abs( myParameters.points[ 1 ] );

    // using sign bit to signal intent, does p=abs(p)?
    if ( p1 == myParameters.points[ 0 ] && p2 == myParameters.points[ 1 ] ) {
        drawAALine( p1.x, p1.y, p2.x, p2.y );
    } else {
        bresenhamLine( p1.x, p1.y, p2.x, p2.y );
    }
}
