/*
 * fbtest3.c
 *
 * http://raspberrycompote.blogspot.ie/2013/01/low-level-graphics-on-raspberry-pi-part_22.html
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
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>

// application entry point
int main(int argc, char* argv[])
{

    int fbfd = 0;
    struct fb_var_screeninfo orig_vinfo;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    char *fbp = 0;


    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (!fbfd) {
      printf("Error: cannot open framebuffer device.\n");
      return(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
      printf("Error reading variable information.\n");
    }
    printf("Original %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, 
	   vinfo.bits_per_pixel );

    // Store for reset (copy vinfo to vinfo_orig)
    memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

    // Change variable info
    vinfo.bits_per_pixel = 8;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
      printf("Error setting variable information.\n");
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
      printf("Error reading fixed information.\n");
    }

    // map fb to user mem 
    screensize = vinfo.xres * vinfo.yres;
    fbp = (char*)mmap(0, 
		      screensize, 
		      PROT_READ | PROT_WRITE, 
		      MAP_SHARED, 
		      fbfd, 
		      0);

    if ((int)fbp == -1) {
      printf("Failed to mmap.\n");
    }
    else {
      // draw...
      int x, y;
      unsigned int pix_offset;

      for (y = 0; y < (vinfo.yres / 2); y++) {
	for (x = 0; x < vinfo.xres; x++) {

	  // calculate the pixel's byte offset inside the buffer
	  // see the image above in the blog...
	  pix_offset = x + y * finfo.line_length;

	  // now this is about the same as fbp[pix_offset] = value
	  *((char*)(fbp + pix_offset)) = 16 * x / vinfo.xres;

	}
      }

      sleep(5);
    }

    // cleanup
    munmap(fbp, screensize);
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
      printf("Error re-setting variable information.\n");
    }
    close(fbfd);

    return 0;
  
  }
