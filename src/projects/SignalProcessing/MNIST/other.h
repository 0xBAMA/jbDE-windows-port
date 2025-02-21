#include "initialWeights.txt"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define INPUT_SIZE 784
#define HIDDEN_SIZE 256
#define OUTPUT_SIZE 10
#define LEARNING_RATE 0.0005f
#define MOMENTUM 0.9f
#define EPOCHS 1
#define IMAGE_SIZE 28
#define TRAIN_SPLIT 0.8

// gcc -O3 -march=native -ffast-math -o nn nn.c -lm

#define TRAIN_IMG_PATH "../../MNIST/train-images-idx3-ubyte"
#define TRAIN_LBL_PATH "../../MNIST/train-labels-idx1-ubyte"

#define TEST_IMG_PATH "../../MNIST/t10k-images-idx3-ubyte"
#define TEST_LBL_PATH "../../MNIST/t10k-labels-idx1-ubyte"

typedef struct {
	float *weights, *biases, *weightMomentum, *biasMomentum;
	int inputSize, outputSize;
} Layer;

typedef struct {
	Layer hidden, output;
} Network;

typedef struct {
	unsigned char *imagesTrain, *labelsTrain;
	int nImagesTrain;

	unsigned char *imagesTest, *labelsTest;
	int nImagesTest;
} InputData;

void softmax( float *input, int size ) {
	float max = input[0], sum = 0;
	for ( int i = 1; i < size; i++ ) {
		if (input[ i ] > max) {
			max = input[ i ];
		}
	}
	for ( int i = 0; i < size; i++ ) {
		input[ i ] = expf(input[ i ] - max);
		sum += input[ i ];
	}
	for ( int i = 0; i < size; i++ ) {
		input[ i ] /= sum;
	}
}

void init_layer( Layer *layer, int inSize, int outSize ) {
	int n = inSize * outSize;
	float scale = sqrtf( 2.0f / inSize );

	layer->inputSize = inSize;
	layer->outputSize = outSize;
	layer->weights = ( float * ) malloc( n * sizeof( float ) );
	layer->biases = ( float * ) calloc( outSize, sizeof( float ) );
	layer->weightMomentum = ( float * ) calloc( n, sizeof( float ) );
	layer->biasMomentum = ( float * ) calloc( outSize, sizeof( float ) );

	static int offset = 0;
	for ( int i = 0; i < n; i++ ) {
		// known set of initial weights
		layer->weights[ i ] = initialWeights[ offset ];
		offset++;

		// layer->weights[ i ] = (( float )rand() / RAND_MAX - 0.5f) * 2 * scale;
		// printf( "%f, ", layer->weights[ i ] );
	}
}

void forward( Layer *layer, float *input, float *output ) {
	for ( int i = 0; i < layer->outputSize; i++ ) {
		output[ i ] = layer->biases[ i ];
	}

	for ( int j = 0; j < layer->inputSize; j++ ) {
		float inputValue = input[ j ];
		for ( int i = 0; i < layer->outputSize; i++ ) {
			output[ i ] += inputValue * layer->weights[ j * layer->outputSize + i ];
		}
	}

	for ( int i = 0; i < layer->outputSize; i++ ) {
		output[ i ] = ( output[ i ] > 0 ) ? output[ i ] : 0;
	}
}

void backward( Layer *layer, float *input, float *outputGradient, float *inputGradient ) {
	if ( inputGradient ) {
		for ( int j = 0; j < layer->inputSize; j++ ) {
			inputGradient[ j ] = 0.0f;
			for ( int i = 0; i < layer->outputSize; i++ ) {
				inputGradient[ j ] += outputGradient[ i ] * layer->weights[ j * layer->outputSize + i ];
			}
		}
	}

	for ( int j = 0; j < layer->inputSize; j++ ) {
		float inputValue = input[ j ];
		for ( int i = 0; i < layer->outputSize; i++ ) {
			float grad = outputGradient[ i ] * inputValue;
			const uint32_t idx = j * layer->outputSize + i;
			layer->weightMomentum[ idx ] = MOMENTUM * layer->weightMomentum[ idx ] + LEARNING_RATE * grad;
			layer->weights[ idx ] -= layer->weightMomentum[ idx ];
			if ( inputGradient ) {
				inputGradient[ j ] += outputGradient[ i ] * layer->weights[ idx ];
			}
		}
	}

	for ( int i = 0; i < layer->outputSize; i++ ) {
		layer->biasMomentum[ i ] = MOMENTUM * layer->biasMomentum[ i ] + LEARNING_RATE * outputGradient[ i ];
		layer->biases[ i ] -= layer->biasMomentum[ i ];
	}
}

float train( Network *net, float *input, int label ) {

	static float finalOutput[ OUTPUT_SIZE ];
	float hiddenOutput[ HIDDEN_SIZE ];
	float outputGradient[ OUTPUT_SIZE ] = { 0 }, hiddenGradient[ HIDDEN_SIZE ] = { 0 };

	forward( &net->hidden, input, hiddenOutput );
	forward( &net->output, hiddenOutput, finalOutput );
	softmax( finalOutput, OUTPUT_SIZE );

	for ( int i = 0; i < OUTPUT_SIZE; i++ ) {
		outputGradient[ i ] = finalOutput[ i ] - ( i == label ? 1.0f : 0.0f );
		printf( "%.4f ( %.4f ), ", finalOutput[ i ], outputGradient[ i ] );
	}

	backward( &net->output, hiddenOutput, outputGradient, hiddenGradient );

	for ( int i = 0; i < HIDDEN_SIZE; i++ ) {
		hiddenGradient[ i ] *= hiddenOutput[ i ] > 0 ? 1 : 0;  // ReLU derivative
	}

	backward( &net->hidden, input, hiddenGradient, NULL );

	return -logf( finalOutput[ label ] + 1e-10f );
}

int predict( Network *net, float *input ) {
	float hiddenOutput[ HIDDEN_SIZE ], finalOutput[ OUTPUT_SIZE ];

	forward( &net->hidden, input, hiddenOutput );
	forward( &net->output, hiddenOutput, finalOutput );
	softmax( finalOutput, OUTPUT_SIZE );

	int maxIndex = 0;
	for ( int i = 1; i < OUTPUT_SIZE; i++ ) {
		if ( finalOutput[ i ] > finalOutput[ maxIndex ] ) {
			maxIndex = i;
		}
	}

	return maxIndex;
}

void read_mnist_images( const char *filename, unsigned char **images, int *nImages ) {
	FILE *file = fopen( filename, "rb" );
	if ( !file ) exit( 1 );

	int temp, rows, cols;
	temp = fread( &temp, sizeof( int), 1, file );
	temp = fread( nImages, sizeof( int), 1, file );
	// *nImages = __builtin_bswap32( *nImages );

	temp = fread( &rows, sizeof( int ), 1, file );
	temp = fread( &cols, sizeof( int ), 1, file );

	// rows = __builtin_bswap32( rows );
	// cols = __builtin_bswap32( cols );

	*images = ( unsigned char * ) malloc( ( *nImages ) * IMAGE_SIZE * IMAGE_SIZE );
	temp = fread( *images, sizeof(unsigned char), ( *nImages ) * IMAGE_SIZE * IMAGE_SIZE, file );
	fclose( file );
}

void read_mnist_labels( const char *filename, unsigned char **labels, int *nLabels ) {
	FILE *file = fopen( filename, "rb" );
	if ( !file ) exit( 1 );

	int temp;
	temp = fread( &temp, sizeof( int ), 1, file );
	temp = fread( nLabels, sizeof( int ), 1, file );
	// *nLabels = __builtin_bswap32(*nLabels);

	*labels = ( unsigned char * ) malloc(  *nLabels );
	temp = fread( *labels, sizeof( unsigned char ), *nLabels, file );
	fclose( file );
}

void shuffle_data(unsigned char *images, unsigned char *labels, int n) {
	for ( int i = n - 1; i > 0; i--) {
		int j = rand() % ( i + 1 );
		for ( int k = 0; k < INPUT_SIZE; k++ ) {
			unsigned char temp = images[ i * INPUT_SIZE + k ];
			images[ i * INPUT_SIZE + k ] = images[ j * INPUT_SIZE + k ];
			images[ j * INPUT_SIZE + k ] = temp;
		}
		unsigned char temp = labels[ i ];
		labels[ i ] = labels[ j ];
		labels[ j ] = temp;
	}
}

void runNetwork() {
	Network net;
	InputData data = { 0 };
	float img[ INPUT_SIZE ];
	clock_t start, end;
	double cpu_time_used;

	srand( time( NULL ) );

	init_layer( &net.hidden, INPUT_SIZE, HIDDEN_SIZE );
	init_layer( &net.output, HIDDEN_SIZE, OUTPUT_SIZE );

	read_mnist_images( TRAIN_IMG_PATH, &data.imagesTrain, &data.nImagesTrain );
	read_mnist_labels( TRAIN_LBL_PATH, &data.labelsTrain, &data.nImagesTrain );

	read_mnist_images( TEST_IMG_PATH, &data.imagesTest, &data.nImagesTest );
	read_mnist_labels( TEST_LBL_PATH, &data.labelsTest, &data.nImagesTest );

	// shuffle_data(data.images, data.labels, data.nImages);

	for ( int epoch = 0; epoch < EPOCHS; epoch++ ) {
		start = clock();
		float total_loss = 0;

		// for ( int i = 0; i < data.nImagesTrain; i++ ) {
		for ( int i = 0; i < 1; i++ ) {
			for ( int k = 0; k < INPUT_SIZE; k++ ) {
				img[ k ] = data.imagesTrain[ i * INPUT_SIZE + k ] / 255.0f;
				printf( "%.4f, ", img[ k ] );
			}
			total_loss += train( &net, img, data.labelsTrain[ i ] );
			printf( "loss: %f\n", total_loss );
		}

		int correct = 0;
		for ( int i = 0; i < data.nImagesTest; i++ ) {
			for ( int k = 0; k < INPUT_SIZE; k++ ) {
				img[ k ] = data.imagesTest[ i * INPUT_SIZE + k ] / 255.0f;
			}
			if ( predict( &net, img ) == data.labelsTest[ i ] ) {
				correct++;
			}
		}

		end = clock();
		cpu_time_used = ( ( double ) ( end - start ) ) / CLOCKS_PER_SEC;

		printf( "Epoch %3d, Accuracy: %.2f%%, Avg Loss: %.8f, Time: %.2f seconds\n", epoch + 1,
			( float ) correct / data.nImagesTest * 100, total_loss / data.nImagesTrain, cpu_time_used );
	}

	free( net.hidden.weights );
	free( net.hidden.biases );
	free( net.hidden.weightMomentum );
	free( net.hidden.biasMomentum );
	free( net.output.weights );
	free( net.output.biases );
	free( net.output.weightMomentum );
	free( net.output.biasMomentum );
	free( data.imagesTrain );
	free( data.labelsTrain );
	free( data.imagesTest );
	free( data.labelsTest );
}
