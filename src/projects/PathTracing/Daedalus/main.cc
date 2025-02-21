#include "daedalus.h"

int main ( int argc, char *argv[] ) {
	Daedalus engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
