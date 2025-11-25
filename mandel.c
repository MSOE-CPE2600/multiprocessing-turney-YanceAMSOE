/*
*  mandel.c
*  Based on example code found here:
*  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
*
*  Converted to use jpg instead of BMP and other minor changes
*  Lab 11: multiprocessing + movie
*  Lab 12: multithreading (pthread)
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include "jpegrw.h"

// local routines
static int iteration_to_color(int i, int max);
static int iterations_at_point(double x, double y, int max);
static void compute_image(imgRawImage *img, double xmin, double xmax,
                          double ymin, double ymax, int max, int tthreads);
static void show_help();

typedef struct {
    imgRawImage *img;
    double xmin, xmax, ymin, ymax;
    int max;
    int start_row;
    int end_row;
} ThreadData;

void *thread_work(void *arg) {
    ThreadData *d = arg;
    int width = d->img->width;

    for (int j = d->start_row; j < d->end_row; j++) {
        for (int i = 0; i < width; i++) {
            double x = d->xmin + i * (d->xmax - d->xmin) / width;
            double y = d->ymin + j * (d->ymax - d->ymin) / d->img->height;
            int iters = iterations_at_point(x, y, d->max);
            setPixelCOLOR(d->img, i, j, iteration_to_color(iters, d->max));
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);  // ensures child prints show immediately

    char c;
    const char *outfile = "mandel";
    double xcenter = 0, ycenter = 0;
    double xscale = 4;
    int image_width = 720;
    int image_height = 480;
    int max = 1000;
    int cprocesses = 1;
    int tthreads = 1;
    double zoom_factor = 1.02;

    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:h:c:z:t:")) != -1) {
        switch (c) {
            case 'x': xcenter = atof(optarg); break;
            case 'y': ycenter = atof(optarg); break;
            case 's': xscale = atof(optarg); break;
            case 'W': image_width = atoi(optarg); break;
            case 'H': image_height = atoi(optarg); break;
            case 'm': max = atoi(optarg); break;
            case 'o': outfile = optarg; break;
            case 'c': cprocesses = atoi(optarg); break;
            case 'z': zoom_factor = atof(optarg); break;
            case 't':
                tthreads = atoi(optarg);
                if (tthreads < 1) tthreads = 1;
                if (tthreads > 20) tthreads = 20;
                break;
            case 'h':
                show_help();
                exit(1);
        }
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);
    int TFrames = 50;

    for (int child = 0; child < cprocesses; child++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (pid == 0) {

            for (int frame = child; frame < TFrames; frame += cprocesses) {

                // Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
                double frame_xscale = xscale / pow(zoom_factor, frame);
                double frame_yscale = frame_xscale * image_height / image_width;

                double xmin = xcenter - frame_xscale / 2.0;
                double xmax = xcenter + frame_xscale / 2.0;
                double ymin = ycenter - frame_yscale / 2.0;
                double ymax = ycenter + frame_yscale / 2.0;

                char framefile[256];
                snprintf(framefile, sizeof(framefile), "%s%03d.jpg", outfile, frame);

                // Display the configuration of the image.
                printf("Child %d PID=%d → %s\n", child, getpid(), framefile);
                printf("mandel: x=%lf y=%lf xscale=%lf yscale=%lf max=%d threads=%d outfile=%s\n",
                       xcenter, ycenter, frame_xscale, frame_yscale, max, tthreads, framefile);

                // Create a raw image of the appropriate size.
                imgRawImage *img = initRawImage(image_width, image_height);

                // Fill it with a black
                setImageCOLOR(img, 0);

                // Compute the Mandelbrot image
                compute_image(img, xmin, xmax, ymin, ymax, max, tthreads);

                // Save the image in the stated file.
                storeJpegImageFile(img, framefile);

                // free the mallocs
                freeRawImage(img);
            }

            _exit(0);
        }
    }

    int parentWait;
    while (wait(&parentWait) > 0);

    gettimeofday(&end, NULL);

    double runtime = (end.tv_sec - start.tv_sec) +
                     (end.tv_usec - start.tv_usec) / 1e6;

    printf("Total runtime: %.3f sec with %d processes and %d threads\n",
           runtime, cprocesses, tthreads);

    return 0;
}

int iterations_at_point(double x, double y, int max) {
    double x0 = x, y0 = y;
    int iter = 0;

    while ((x * x + y * y <= 4) && iter < max) {
        double xt = x * x - y * y + x0;
        double yt = 2 * x * y + y0;
        x = xt;
        y = yt;
        iter++;
    }
    return iter;
}

void compute_image(imgRawImage *img, double xmin, double xmax,
                   double ymin, double ymax, int max, int tthreads) {

    pthread_t threads[20];
    ThreadData td[20];

    int height = img->height;
    int base = height / tthreads;
    int extra = height % tthreads;
    int row = 0;

    for (int t = 0; t < tthreads; t++) {
        int rows = base + (t < extra ? 1 : 0);

        td[t].img = img;
        td[t].xmin = xmin;
        td[t].xmax = xmax;
        td[t].ymin = ymin;
        td[t].ymax = ymax;
        td[t].max = max;
        td[t].start_row = row;
        td[t].end_row = row + rows;

        row += rows;
        pthread_create(&threads[t], NULL, thread_work, &td[t]);
    }

    for (int t = 0; t < tthreads; t++) {
        pthread_join(threads[t], NULL);
    }
}

int iteration_to_color(int iters, int max) {
    int color = 0x3333FF + (0xFFFFFF * iters / (double)max);
    return color;
}

void show_help() {
    printf("Use: mandel [options]\n");
    printf("-m <max>    Max iterations\n");
    printf("-x <coord>  X center\n");
    printf("-y <coord>  Y center\n");
    printf("-s <scale>  X scale\n");
    printf("-W <pixels> Width\n");
    printf("-H <pixels> Height\n");
    printf("-o <file>   Output filename prefix\n");
    printf("-c <procs>  Number of child processes\n");
    printf("-t <thrds>  Number of threads (1–20)\n");
    printf("-z <zoom>   Zoom factor per frame\n");
}
