#ifndef TIMER
#define TIMER

#include <chrono>
#include <sstream>
#include <iostream>
#include <algorithm>

#include <math.h>

inline std::string timeDateString () {
	auto now = std::chrono::system_clock::now();
	auto inTime_t = std::chrono::system_clock::to_time_t( now );
	std::stringstream ssA;
	ssA << std::put_time( std::localtime( &inTime_t ), "%Y-%m-%d at %H-%M-%S" );
	return ssA.str();
}

inline std::string fixedWidthTimeString () {
	auto now = std::chrono::system_clock::now();
	auto inTime_t = std::chrono::system_clock::to_time_t( now );
	std::stringstream ssA;
	ssA << std::put_time( std::localtime( &inTime_t ), "[%H:%M:%S]" );
	return ssA.str();
}

//=============================================================================
//==== CLI Progress Bar Utility ===============================================
//=============================================================================

class progressBar {
public:
	progressBar () {
		// set report width
		// set the label for the progress bar
		// set the chars that are used to done/undone
		// set the total amount of shit to do
		// set the amount of shit done now
		// set the time that the process started? or skip timing entirely for now?
	}

	float done = 0.0f;
	float total = 1.0f;

	float getFraction () {
		return std::clamp( float( done ) / float( total ), 0.0f, 1.0f );
	}

	string getFilledBarPortion () {
		stringstream ss;
		const float frac = getFraction();
		int numFill = std::floor( reportWidth * frac ) - 1;
		for( int i = 0; i <= numFill; i++ )
			ss << doneChar;
		return ss.str();
	}

	string getEmptyBarPortion () {
		stringstream ss;
		const float frac = getFraction();
		int numFill = std::floor( reportWidth * frac ) - 1;
		for ( int i = 0; i < reportWidth - numFill - 1; i++ )
			ss << undoneChar;
		return ss.str();
	}

	string getPercentageString () {
		stringstream ss;
		const float frac = getFraction();
		ss << fixedWidthNumberString( std::floor( 100.0f * frac ), 3, ' ' ) << "." << fixedWidthNumberString( int( 10000.0f * frac ) - std::floor( 100.0f * frac ) * 100, 2 );
		return ss.str();
	}

	string currentState () {
		stringstream ss;
		// const float frac = getFraction();
		// int numFill = std::floor( reportWidth * frac ) - 1;

		// draw the bar
		ss << label << "[";
		ss << getFilledBarPortion();
		ss << getEmptyBarPortion();
		ss << "] ";

		// and report the percentage
		ss << getPercentageString() << "%";
		return ss.str();
	}

	void writeCurrentState () {
		// calculate completion percentage
		cout << "\r" << currentState() << "                       ";

		// and timing if desired
		// if ( reportTime )
			// cout << " in " << Tock() / 1000.0f << "s" << std::flush;
	}

	char doneChar = '=';
	char undoneChar = '.';
	int reportWidth = 64;
	bool reportTime = false;

	std::string label = std::string( " Progress: " );
};

//=============================================================================
//==== OpenGL Timer Query Wrapper =============================================
//=============================================================================

struct queryPair_GPU {
	queryPair_GPU ( string s ) : label( s ) {}
	string label;
	GLuint queryID[ 2 ];
	float result = 0.0f;
};

struct queryPair_CPU {
	queryPair_CPU ( string s ) : label( s ) {}
	string label;
	std::chrono::time_point< std::chrono::system_clock > tStart;
	std::chrono::time_point< std::chrono::system_clock > tStop;
	float result;
};

class timerManager {
public:
	std::vector < queryPair_GPU > queries_GPU;
	std::vector < queryPair_CPU > queries_CPU;
	void gather () {
		for ( auto& q : queries_GPU ) {
			GLint timeAvailable = 0;
			while ( !timeAvailable ) { // wait on the most recent of the queries to become available
				glGetQueryObjectiv( q.queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
			}

			GLuint64 startTime, stopTime; // get the query results, since they're both ready
			glGetQueryObjectui64v( q.queryID[ 0 ], GL_QUERY_RESULT, &startTime );
			glGetQueryObjectui64v( q.queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
			glDeleteQueries( 2, &q.queryID[ 0 ] ); // and then delete them

			// get final operation time in ms, from difference of nanosecond timestamps
			q.result = ( stopTime - startTime ) / 1000000.0f;
		}
	}

	void clear () { // prepare for next frame
		queries_GPU.clear();
		queries_CPU.clear();
	}
};

class unscopedTimer {
public:
	queryPair_GPU q;

	std::chrono::time_point< std::chrono::steady_clock > tStart;
	std::chrono::time_point< std::chrono::steady_clock > tStop;

	unscopedTimer () : q( " " ) {}
	void tick () { // start the timers
		// GPU
		glGenQueries( 2, &q.queryID[ 0 ] );
		glQueryCounter( q.queryID[ 0 ], GL_TIMESTAMP );

		// CPU
		tStart = std::chrono::steady_clock::now();
	}

	void tock () { // end the timers
		// GPU
		glQueryCounter( q.queryID[ 1 ], GL_TIMESTAMP );
		GLint timeAvailable = 0;
		while ( !timeAvailable ) { // wait on the most recent of the queries to become available
			glGetQueryObjectiv( q.queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
		}

		GLuint64 startTime, stopTime; // get the query results, since they're both ready
		glGetQueryObjectui64v( q.queryID[ 0 ], GL_QUERY_RESULT, &startTime );
		glGetQueryObjectui64v( q.queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
		glDeleteQueries( 2, &q.queryID[ 0 ] ); // and then delete them

		// get final operation time in ms, from difference of nanosecond timestamps
		timeGPU = ( stopTime - startTime ) / 1000000.0f;

		// CPU
		tStop = std::chrono::steady_clock::now();
		timeCPU = std::chrono::duration_cast<std::chrono::microseconds>( tStop - tStart ).count() / 1000.0f;
	}

	// values in ms
	float timeGPU;
	float timeCPU;
};

inline timerManager* timerQueries;

class scopedTimer {
public:
	queryPair_CPU c;
	queryPair_GPU q;
	scopedTimer ( string label ) : c ( label ), q ( label ) {
		// GPU query prep
		glGenQueries( 2, &q.queryID[ 0 ] );
		glQueryCounter( q.queryID[ 0 ], GL_TIMESTAMP );

		// CPU query prep
		c.tStart = std::chrono::system_clock::now();
	}
	~scopedTimer () {
		// GPU query finish
		glQueryCounter( q.queryID[ 1 ], GL_TIMESTAMP );
		timerQueries->queries_GPU.push_back( q );

		// CPU query finish
		c.tStop = std::chrono::system_clock::now();
		c.result = std::chrono::duration_cast<std::chrono::microseconds>( c.tStop - c.tStart ).count() / 1000.0f;
		timerQueries->queries_CPU.push_back( c );
	}
};

// Timing for the initialization code, similar to scoped timers but outputs to CLI
class Block {
const int reportWidth = 40;
public:
	std::string myName;
	std::chrono::time_point<std::chrono::steady_clock> tStart;
	Block( string blockName ) {
		myName = blockName;
		tStart = std::chrono::steady_clock::now();
		cout << T_BLUE << "    " << blockName << RESET << "... " << endl;
	}

	~Block() {
		const float timeInMS = std::chrono::duration_cast< std::chrono::microseconds >( std::chrono::steady_clock::now() - tStart ).count() / 1000.0f;
		const float wholeMS = int( std::floor( timeInMS ) );
		const float partialMS = int( ( timeInMS - wholeMS ) * 1000.0f );
		cout << "        ";
		// for ( unsigned int i = 0; i < reportWidth - myName.size(); i++ ) {
		for ( int i = 0; i < reportWidth; i++ ) {
			cout << ".";
		}
		cout << T_GREEN << "done." << T_RED << " ( " << std::setfill( ' ' ) << std::setw( 6 ) << wholeMS << "."
			<< std::setw( 3 ) << std::setfill( '0' ) << partialMS << " ms )" << RESET << newline;
	}
};

#endif
