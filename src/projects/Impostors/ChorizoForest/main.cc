#include "chorizo.h"

int main ( int argc, char *argv[] ) {
	Chorizo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
