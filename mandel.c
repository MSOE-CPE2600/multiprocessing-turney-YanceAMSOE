/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <math.h>
#include "jpegrw.h"

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max );
static void show_help();
//static int RandHex();


int main( int argc, char *argv[] ) {
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.
	const char *outfile = "mandel";
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	// double yscale = 0; // calc later
	int    image_width = 720;
	int    image_height = 480;
	int    max = 1000;
	int    cprocesses = 1; // Amount of children processes default 1

	//for the zoom factor for the mandle movie
	double zoom_factor = 1.02;

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:h:c:z"))!=-1) {
		
		switch(c) 
		{
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'c':
				cprocesses = atoi(optarg);
				break;	
			case 'z':
				zoom_factor = atof(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}
	//time before the child process start
	struct timeval start, end;
	gettimeofday(&start, NULL);

	//Set the variable for amount of frams
	int TFrames = 50; //Total Frames

	// fork this amount of children 
	for (int child = 0; child <cprocesses; child++) {

		pid_t pid = fork();

		if (pid < 0) {
			perror("fork failed");
			exit(1);
		}

		if (pid == 0) {
			for(int frame = child; frame < TFrames; frame += cprocesses) {
				// Scale changes each frame for zoom animation
				// Each frame zooms in from the previous one by dividing by zoom_factor
				// Frame 0: full scale, Frame 1: scale/zoom_factor, Frame 2: scale/zoom_factor^2, etc.
				double frame_xscale = xscale / pow(zoom_factor, frame);
				
				// Calculate y scale to maintain aspect ratio (preserve image proportions)
				double frame_yscale = frame_xscale * image_height / image_width;

				// Calculate coordinate bounds centered on (xcenter, ycenter)
				double xmin = xcenter - frame_xscale / 2.0;
				double xmax = xcenter + frame_xscale / 2.0;
				double ymin = ycenter - frame_yscale / 2.0;
				double ymax = ycenter + frame_yscale / 2.0;

				// Create output filename with zero-padding for video tools
				char framefile[256];
				snprintf(framefile, sizeof(framefile), "%s%03d.jpg", outfile, frame);

				printf("Child %d PID=%d → %s\n", child, getpid(), framefile);

				// Display the configuration of the image.
				printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",
					xcenter, ycenter, frame_xscale, frame_yscale, max, framefile);

				// Create a raw image of the appropriate size.
				imgRawImage* img = initRawImage(image_width, image_height);

				// Fill it with a black
				setImageCOLOR(img,0);

				// Compute the Mandelbrot image
				compute_image(img, xmin, xmax, ymin, ymax, max);

				// Save the image in the stated file.
				storeJpegImageFile(img, framefile);

				// free the mallocs
				freeRawImage(img);
			}
			_exit(0);
		}
	}

	int parentWait;
	while(wait(&parentWait) > 0);
	gettimeofday(&end, NULL);

	double runtime = (end.tv_sec - start.tv_sec) + 
					 (end.tv_usec - start.tv_usec) / 1e6;
	
	printf("Total runtime: %.3f sec with %d processes\n", runtime, cprocesses);
	return 0;
}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max ) {
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;
		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max ) {
	int i,j;

	int width = img->width;
	int height = img->height;

	// For every pixel

	for(j=0;j<height;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
		}
	}
}


/*
Convert an iteration number to a color.
Points that never escape (iters >= max) are black so the Mandelbrot shape is visible.
Other points get a smooth, colorful gradient based on escape speed.
*/
int iteration_to_color( int iters, int max )
{
	for(int i = 0; i < max; i++){
		
	int color = 0x3333FF + (0xFFFFFF*iters/(double)max); //Variable used to be 0xFFFFFF
	return color;
	}
	return rand() * 0xFFFFFF;
}

//We have to make 0xFFFFFF a random number from 0 to 16777215
//int RandHex(){
// 	(rgb&0xFF0000)>>16,(rgb&0xFF00)>>8,rgb&0xFF) is how the other program convers the hex value to get the colors
//	return rand() % 0xFFFFFF;

//}


// Show help message
void show_help() {
	printf("Use: mandel [options]\n");
	printf("Options For Mandel:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("-c          Set the amount of children processes you want to run. (default = 1)\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
