/*
 * ppmtofbimg.c
 *
 * Converts a 24 bit P6 PPM file to 'raw' RGB 16 bit 5:6:5 format
 * and draws into framebuffer
 *
 * http://raspberrycompote.blogspot.com/2016/02/low-level-graphics-on-raspberry-pi-more_24.html
 *
 * To build:
 *   gcc -O2 -o ppmtofbimg ppmtofbimg.c
 *
 * Usage:
 *   - make sure you have a 24 bit PPM to begin with and the image
 *     (pixel dimensions) is smaller than the screen (see test24.ppm)
 *   - to run
 *        ./ppmtofbimg test24.ppm
 *   - draws the given image to the upper left corner of screen
 *
 * Original work by J-P Rosti (a.k.a -rst- and 'Raspberry Compote')
 *
 * Licensed under the Creative Commons Attribution 3.0 Unported License
 * (http://creativecommons.org/licenses/by/3.0/deed.en_US)
 *
 * Distributed in the hope that this will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/ioctl.h>
#include <signal.h>

// 'global' variables to store screen info
int fbfd = 0;
char *fbp = 0;
long int page_size = 0;
int cur_page = 0;
struct fb_var_screeninfo orig_vinfo;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
int kbfd = 0;
struct fb_image image;

int read_ppm(char *fpath, struct fb_image *image) {

	int errval = 0;
	FILE* fp = 0;
    int bytes_per_pixel = 2; // 16 bit

    fp = fopen(fpath, "r");
    if (fp == 0) {
        errval = errno;
        fprintf(stderr, "Error opening file %s (errno=%d).\n", errval);
        return errval;
    }

	char magic[2];
	int width = -1;
	int height = -1;
	int depth = -1;

	if ( (fread(magic, 2, 1, fp) == 1)
		&& (memcmp("P6", magic, 2) == 0) )
	{
		//fprintf(stderr, "Got P6 ppm.\n");

		if (fscanf(fp, "%d %d\n", &width, &height) == 2) {
			//fprintf(stderr, "w=%d, h=%d\n", width, height);
		}
		else {
			fprintf(stderr, "Read size failed.\n");
			width = height = -1;
            errval = EINVAL;
		}

		if (fscanf(fp, "%d\n", &depth) == 1) {
			//fprintf(stderr, "d=%d\n", depth);
		}
		else
		{
			fprintf(stderr, "Read depth failed.\n");
			depth = -1;
            errval = EINVAL;
		}

		if (depth != 255) {
			fprintf(stderr, "Only 255 depth supported.\n");
			depth = -1;
            errval = EINVAL;
		}

	}
	else {
		fprintf(stderr, "Not a P6 ppm.\n");
        errval = EINVAL;
	}

	if ( (width > -1) && (height > -1) && (depth = -1) ) {
		// header read ok

        image->dx = 0;
        image->dy = 0;
        image->width = width;
        image->height = height;
        image->fg_color = 0;
        image->bg_color = 0;
        image->depth = 16;

        // allocate memory
        image->data = malloc(width * height * bytes_per_pixel);
        if (image->data == 0) {
            fprintf(stderr, "Failed to allocate memory.\n");
            errval = ENOMEM;
        }
        else {
            // read
            int y;
            int x;
            unsigned char rgb[3];

            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    if (fread(rgb, 3, 1, fp) == 1) {
                        unsigned char r = rgb[0];
                        unsigned char g = rgb[1];
                        unsigned char b = rgb[2];
                        unsigned short rgb565 = ((r >> 3) << 11) + ((g >> 2) << 5) + (b >> 3);
                        // store pixel in memory
                        unsigned int pix_offset = (y * width + x ) * bytes_per_pixel;
                        *((unsigned short *)(image->data + pix_offset)) = rgb565;
                    }
                    else {
                        errval = errno;
                        fprintf(stderr, "Read data failed (errno=%d).\n", errval);
                        break;
                    }
                }
                if (errval != 0)
                    break;
            }
        }
	}

    fclose(fp);

    return errval;
}

// draw
void draw(struct fb_image *image) {
    int y;
    int x;

    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            // get pixel from image
            unsigned int img_pix_offset = (y * image->width + x) * 2;
            unsigned short c = *(unsigned short *)(image->data + img_pix_offset);
            // plot pixel to screen
            unsigned int fb_pix_offset = x * 2 + y * finfo.line_length;
            *((unsigned short*)(fbp + fb_pix_offset)) = c;
        }
    }
}

// cleanup
void cleanup() {
    // reset cursor
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_TEXT);
        close(kbfd);
    }
    // unmap fb file from memory
    munmap(fbp, finfo.smem_len);
    // reset the display mode
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
        printf("Error re-setting variable information.\n");
    }
    // close fb file
    close(fbfd);
    // free image data
    free((void *)image.data);
}

// signal handler to handle Ctrl+C
void sig_handler(int signo) {
    cleanup();
    exit(signo);
}

// application entry point
int main(int argc, char* argv[])
{

    // read the image file
    int ret = read_ppm(argv[1], &image);
    if (ret != 0) {
        printf("Reading image failed.\n");
        return ret;
    }

    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
      printf("Error: cannot open framebuffer device.\n");
      return(1);
    }

    // set up signal handler to handle Ctrl+C
    signal(SIGINT, sig_handler);

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
      printf("Error reading variable information.\n");
    }

    // Store for reset (copy vinfo to vinfo_orig)
    memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
      printf("Error reading fixed information.\n");
    }
    page_size = finfo.line_length * vinfo.yres;

    // hide cursor
    kbfd = open("/dev/tty", O_WRONLY);
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_GRAPHICS);
    }

    // map fb to user mem
    fbp = (char*)mmap(0, 
              finfo.smem_len, 
              PROT_READ | PROT_WRITE, 
              MAP_SHARED, 
              fbfd, 
              0);

    if ((int)fbp == -1) {
        printf("Failed to mmap.\n");
    }
    else {
        // draw...
        draw(&image);
        sleep(2);
    }

    cleanup();

    return 0;

}
