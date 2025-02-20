/* 
 *	dynadraw -
 *
 *	     Use a simple dynamics model to create calligraphic strokes.
 *
 *
	To compile:
		cc dynadraw.c -o dynadraw -lgl -lm
 *
 *	leftmouse   - used for drawing
 *	middlemouse - clears page
 *	rightmouse  - menu
 *
 *	uparrow     - wider strokes
 *	downarrow   - narrower strokes
 *
 *				Paul Haeberli - 1989
 *		
 */
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <gl/gl.h>
#include <gl/device.h>

#define SLIDERHIGH	(15)
#define SLIDERLEFT	(200)
#define TIMESLICE	(0.005)
#define MAXPOLYS	(50000)

typedef struct filter {
	float curx, cury;
	float velx, vely, vel;
	float accx, accy, acc;
	float angx, angy;
	float mass, drag;
	float lastx, lasty;
	int fixedangle;
} filter;

float initwidth = 1.5f;
float width;
float odelx, odely;
float curmass, curdrag;
float polyverts[ 4 * 2 * MAXPOLYS ];
int npolys;
long xsize, ysize;
long xorg, yorg;
filter mouse;

float flerp();
float fgetmousex();
float fgetmousey();
float gettime();
unsigned long getltime();

filtersetpos( f, x, y )
filter *f;
float x, y;
{
	f->curx = x;
	f->cury = y;
	f->lastx = x;
	f->lasty = y;
	f->velx = 0.0f;
	f->vely = 0.0f;
	f->accx = 0.0f;
	f->accy = 0.0f;
}

filterapply( f, mx, my )
filter *f;
float mx, my;
{
	float mass, drag;
	float fx, fy, force;

/* calculate mass and drag */
	mass = flerp( 1.0f, 160.0f, curmass);
	drag = flerp( 0.00f, 0.5f, curdrag * curdrag );

/* calculate force and acceleration */
	fx = mx - f->curx;
	fy = my - f->cury;
	f->acc = sqrt( fx * fx + fy * fy );
	if ( f->acc < 0.000001f )
		return 0; // computed stroke length goes to zero for low acceleration
	f->accx = fx / mass;
	f->accy = fy / mass;

/* calculate new velocity */
	f->velx += f->accx;
	f->vely += f->accy;
	f->vel = sqrt( f->velx * f->velx + f->vely * f->vely );
	f->angx = -f->vely;
	f->angy = f->velx;
	if ( f->vel < 0.000001f )
		return 0; // computed stroke length goes to zero for low velocity

/* calculate angle of drawing tool */
	f->angx /= f->vel;
	f->angy /= f->vel;
	if( f->fixedangle ) {
		f->angx = 0.6f;
		f->angy = 0.2f;
	}

/* apply drag */
	f->velx = f->velx * ( 1.0f - drag );
	f->vely = f->vely * ( 1.0f - drag );

/* update position */
	f->lastx = f->curx;
	f->lasty = f->cury;
	f->curx = f->curx + f->velx;
	f->cury = f->cury + f->vely;
	return 1;
}


// get fractional screen position, with clamping, used for sliders
float paramval () {
	float p = ( float ) ( getmousex() - SLIDERLEFT ) / ( xsize - SLIDERLEFT );
	if ( p < 0.0f )
		return 0.0f;
	if ( p > 1.0f )
		return 1.0f;
	return p;
}

main () {
    short val;
    int menu, pres;
    float p, mx, my;

    curmass = 0.5;
    curdrag = 0.15;
    prefsize(800,640);
    initbuzz();
    winopen("dynadraw" );
    glcompat(GLC_OLDPOLYGON,1);
    subpixel(1);
    shademodel(FLAT);
    gconfig();
    qdevice(LEFTMOUSE);
    qdevice(MIDDLEMOUSE);
    qdevice(MENUBUTTON);
    qdevice(RIGHTSHIFTKEY);
    qdevice(UPARROWKEY);
    qdevice(DOWNARROWKEY);
    qdevice(RIGHTSHIFTKEY);
    makeframe();
    menu = defpup("dynadraw %t|toggle line style|save PostScript" );
    width = initwidth;
    mouse.fixedangle = 1;
	while( 1 ) {
		switch( qread( &val ) ) {
			case REDRAW:
			makeframe();
			break;

			case MIDDLEMOUSE:
			if ( val )
				clearscreen();
			break;

		case UPARROWKEY:
			if ( val )
				initwidth *= 1.414213f;
			width = initwidth;
			break;

		case DOWNARROWKEY:
			if ( val )
				initwidth /= 1.414213f;
			width = initwidth;
			break;

		case MENUBUTTON:
			if( val ) {
				switch( dopup( menu ) ) {
					case 1:
					mouse.fixedangle = 1-mouse.fixedangle;
					break;

					case 2:
					savepolys();
					break;
				}
			}
			break;

		case LEFTMOUSE:
			if(val) {
				my = getmousey();
				if( my > 0 * SLIDERHIGH && my < 2 * SLIDERHIGH ) {
					if( my > SLIDERHIGH ) {
						while( getbutton( LEFTMOUSE ) ) {
							p = paramval();
							if( p != curmass ) {
								curmass = p;
								showsettings();
							}
						}
					} else {
						while( getbutton( LEFTMOUSE ) ) {
							p = paramval();
							if( p != curdrag ) {
								curdrag = p;
								showsettings();
							}
						}
					}
				} else {
					mx = 1.25f * fgetmousex();
					my = fgetmousey();
					filtersetpos( &mouse, mx, my );
					odelx = 0.0f;
					odely = 0.0f;
					while( getbutton( LEFTMOUSE ) ) {
						mx = 1.25f * fgetmousex();
						my = fgetmousey();
						if( filterapply( &mouse,mx,my ) ) {
							drawsegment( &mouse );
							color(0);
							buzz();
						}
					}
				}
			}
			break;
		}
	}
}

makeframe () {
	reshapeviewport();
	getsize(&xsize,&ysize);
	getorigin(&xorg,&yorg);
	clearscreen();
}

clearscreen () {
	int x, y;

	ortho2(0.0,1.25,0.0,1.0);
	color(51);
	setpattern(0);
	clear();
	npolys = 0;
	showsettings();
	color(0);
}

showsettings () {
	char str[256];
	int xpos;

	ortho2(-0.5,xsize-0.5,-0.5,ysize-0.5);
	color(51);
	rectfi(0,0,xsize,2*SLIDERHIGH);
	color(0);
	sprintf(str, "Mass %g",curmass); 
	cmov2i(20,3+1*SLIDERHIGH);
	charstr(str);
	sprintf(str, "Drag %g",curdrag); 
	cmov2i(20,3+0*SLIDERHIGH);
	charstr(str);
	move2i(SLIDERLEFT,0);
	draw2i(SLIDERLEFT,2*SLIDERHIGH);
	move2i(0,1*SLIDERHIGH);
	draw2i(xsize,1*SLIDERHIGH);
	move2i(0,2*SLIDERHIGH);
	draw2i(xsize,2*SLIDERHIGH);
	color(1);
	xpos = SLIDERLEFT+curmass*(xsize-SLIDERLEFT);
	rectfi(xpos,1*SLIDERHIGH,xpos+4,2*SLIDERHIGH);
	xpos = SLIDERLEFT+curdrag*(xsize-SLIDERLEFT);
	rectfi(xpos,0*SLIDERHIGH,xpos+4,1*SLIDERHIGH);
	ortho2(0.0,1.25,0.0,1.0);
}

drawsegment(f)
filter *f;
{
	float delx, dely;
	float wid, *fptr;
	float px, py, nx, ny;

	wid = 0.04f - f->vel;
	wid = wid * width;
	if( wid < 0.00001f ) {
		wid = 0.00001f;
	}
	delx = f->angx*wid;
	dely = f->angy*wid;

	color(0);
	px = f->lastx;
	py = f->lasty;
	nx = f->curx;
	ny = f->cury;

	fptr = polyverts+8*npolys;
	bgnpolygon();
	fptr[ 0 ] = px+odelx;
	fptr[ 1 ] = py+odely;
	v2f( fptr );
	fptr += 2;
	fptr[ 0 ] = px-odelx;
	fptr[ 1 ] = py-odely;
	v2f( fptr );
	fptr += 2;
	fptr[ 0 ] = nx-delx;
	fptr[ 1 ] = ny-dely;
	v2f( fptr );
	fptr += 2;
	fptr[ 0 ] = nx+delx;
	fptr[ 1 ] = ny+dely;
	v2f( fptr );
	fptr += 2;
	endpolygon();
	npolys++;
	if( npolys >= MAXPOLYS ) {
		fprintf(stderr, "out of polys - increase the define MAXPOLYS\n" );
		npolys--;
	}
	fptr -= 8;
	bgnclosedline();
	v2f( fptr );
	fptr += 2;
	v2f( fptr );
	fptr += 2;
	v2f( fptr );
	fptr += 2;
	v2f( fptr );
	fptr += 2;
	endclosedline();
	odelx = delx;
	odely = dely;
}

int buzztemp, buzzmax;

/* this returns the time in 100ths of a sec since the system was rebooted  */
unsigned long getltime () {
	struct tms ct;
	return times( &ct );
}

/* this should spin for TIMESLICE seconds afteer "calibration" */
buzz () {
	int i;
	for(i=0; i<buzzmax; i++)
		buzztemp++;
}

/* this "calibrates" the buzz loop to determine buzzmax */
initbuzz () {
	long t0, t1;

	buzzmax = 1000000;
	sginap(10);	/* sleep for 10/100 of a second */
	t0 = getltime();
	buzz();
	t1 = getltime();
	buzzmax = TIMESLICE*(100.0*1000000.0)/(t1-t0);
}

savepolys () {
	FILE *of;
	int i;
	float *fptr;

	of = fopen( "dynadraw.ps", "w" );
	if ( !of ) {
		fprintf( stderr, "dynadraw: can't open out put file [dynadraw.ps]\n" );
		return;
	}
	fprintf( of, "%%!\n" );
	fprintf( of, "%f %f translate\n",0.25*72,(11.0-0.75)*72 );
	fprintf( of, "-90 rotate\n" );
	fprintf( of, "%f %f scale\n",8.0*72,8.0*72 );
	fprintf( of, "0.0 setgray\n" );
	printf( "npolys %d\n", npolys );
	fptr = polyverts;
	for( i = 0; i < npolys; i++ ) {
		fprintf( of, "newpath\n" );
		fprintf( of, "%f %f moveto\n", fptr[ 0 ], fptr[ 1 ] );
		fptr += 2;
		fprintf( of, "%f %f lineto\n", fptr[ 0 ], fptr[ 1 ] );
		fptr += 2;
		fprintf( of, "%f %f lineto\n", fptr[ 0 ], fptr[ 1 ] );
		fptr += 2;
		fprintf( of, "%f %f lineto\n", fptr[ 0 ], fptr[ 1 ] );
		fptr += 2;
		fprintf( of, "closepath\n" );
		fprintf( of, "fill\n" );
	}
	fprintf( of, "showpage\n" );
	fclose(of);
	fprintf(stderr, "PostScript saved to dynadraw.ps\n" );
}

/*
 *	winlib routines follow
 *
 */
float fgetmousex () {
	return ((float)getvaluator(MOUSEX)-xorg)/(float)xsize;
}

float fgetmousey () {
	return ((float)getvaluator(MOUSEY)-yorg)/(float)ysize;
}

int getmousex () {
	return getvaluator(MOUSEX)-xorg;
}

int getmousey () {
	return getvaluator(MOUSEY)-yorg;
}

float flerp(f0,f1,p)
float f0, f1, p;
{
	return ( ( f0 * ( 1.0 - p ) ) + ( f1 * p ) );
}

savewindow(name)
char *name;
{
	char cmd[256];
	long xorg, yorg;
	long xsize, ysize;

	getorigin(&xorg,&yorg);
	getsize(&xsize,&ysize);
	sprintf(cmd, "scrsave %s %d %d %d %d\n",
			name,xorg,xorg+xsize-1,yorg,yorg+ysize-1);
	system(cmd);
}

