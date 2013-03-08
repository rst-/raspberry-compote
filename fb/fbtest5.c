/*
 * fbtest5.c
 *
 * http://raspberrycompote.blogspot.ie/2013/03/low-level-graphics-on-raspberry-pi-part_7.html
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

// default framebuffer palette
typedef enum {
  BLACK        =  0, /*   0,   0,   0 */
  BLUE         =  1, /*   0,   0, 172 */
  GREEN        =  2, /*   0, 172,   0 */
  CYAN         =  3, /*   0, 172, 172 */
  RED          =  4, /* 172,   0,   0 */
  PURPLE       =  5, /* 172,   0, 172 */
  ORANGE       =  6, /* 172,  84,   0 */
  LTGREY       =  7, /* 172, 172, 172 */
  GREY         =  8, /*  84,  84,  84 */
  LIGHT_BLUE   =  9, /*  84,  84, 255 */
  LIGHT_GREEN  = 10, /*  84, 255,  84 */
  LIGHT_CYAN   = 11, /*  84, 255, 255 */
  LIGHT_RED    = 12, /* 255,  84,  84 */
  LIGHT_PURPLE = 13, /* 255,  84, 255 */
  YELLOW       = 14, /* 255, 255,  84 */
  WHITE        = 15  /* 255, 255, 255 */
} COLOR_INDEX_T;

static unsigned short def_r[] = 
    { 0,   0,   0,   0, 172, 172, 172, 168,  
     84,  84,  84,  84, 255, 255, 255, 255};
static unsigned short def_g[] = 
    { 0,   0, 168, 168,   0,   0,  84, 168,  
     84,  84, 255, 255,  84,  84, 255, 255};
static unsigned short def_b[] = 
    { 0, 172,   0, 168,   0, 172,   0, 168,  
     84, 255,  84, 255,  84, 255,  84, 255};


// 'global' variables to store screen info
char *fbp = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

// helper function to 'plot' a pixel in given color
void put_pixel(int x, int y, int c)
{
    // calculate the pixel's byte offset inside the buffer
    unsigned int pix_offset = x + y * finfo.line_length;

    // now this is about the same as 'fbp[pix_offset] = value'
    *((char*)(fbp + pix_offset)) = c;

}

// helper function for drawing - no more need to go mess with
// the main function when just want to change what to draw...
void draw() {

    int x, y;

    for (y = 0; y < (vinfo.yres / 2); y++) {
        for (x = 0; x < vinfo.xres; x++) {

            // color based on the 16th of the screen width
            int c = 16 * x / vinfo.xres;
    
            // default colors at upper half
            put_pixel(x, y, c);
            // our own colors at lower half
            put_pixel(x, y + (vinfo.yres / 2), c + 16);

        }
    }

}

// application entry point
int main(int argc, char* argv[])
{

    int fbfd = 0;
    struct fb_var_screeninfo orig_vinfo;
    long int screensize = 0;


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

    // Set palette
    unsigned short r[256];
    unsigned short g[256];
    unsigned short b[256];
    memset(&r, 0, 256); // initialise with zeros
    memset(&g, 0, 256);
    memset(&b, 0, 256);
    int i;
    for(i = 0; i < 16; i++) {
        // copy the hard-coded values
        // note that Linux provides more precision (0-65535),
        // so we multiply ours (0-255) by 256
        r[i] = def_r[i] << 8;
        g[i] = def_g[i] << 8;
        b[i] = def_b[i] << 8;
    } 
    struct fb_cmap pal;
    pal.start = 16; // start our colors after the default 16
    pal.len = 256; // kludge to force bcm fb drv to commit palette...
    pal.red = r;
    pal.green = g;
    pal.blue = b;
    pal.transp = 0; // we want all colors non-transparent == null
    if (ioctl(fbfd, FBIOPUTCMAP, &pal)) {
        printf("Error setting palette.\n");
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
        draw();
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
