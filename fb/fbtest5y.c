/*
 * fbtest5y.c
 *
 * http://raspberrycompote.blogspot.com/2016/02/low-level-graphics-on-raspberry-pi-more.html
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

// helper function for drawing
void draw() {

    int x, y;
    int t = 20;

    // t x t pix black and blue checkerboard
    for (y = 0; y < vinfo.yres; y++) {
        int xoffset = y / t % 2;
        for (x = 0; x < vinfo.xres; x++) {

            // color based on the tile
            int c = (x / t + xoffset) % 2;

            // draw pixel
            put_pixel(x, y, c);

        }
    }
    // colored blocks
    for (y = 0; y < vinfo.yres; y += t) {
        int xoffset = y / t % 2;
        for (x = t * xoffset; x < vinfo.xres; x += t * 2) {
            int x2, y2;
            for (y2 = 0; y2 < t; y2++) {
                for (x2 = 0; x2 < t; x2++) {

                    // color based on y2 value
                    // using the custom colors (16+)
                    int c = 16 + (y2 % 16);

                    // draw pixel
                    put_pixel(x + x2, y + y2, c);

                }
            }
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
    if (fbfd == -1) {
      printf("Error: cannot open framebuffer device.\n");
      return(1);
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
      printf("Error reading variable information.\n");
    }

    // Store for reset (copy vinfo to vinfo_orig)
    memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

    // Change variable info
    vinfo.bits_per_pixel = 8; // 8-bit mode
    // drop resolution
    while (vinfo.xres > 800) {
        vinfo.xres /= 2;
        vinfo.yres /= 2;
    }
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo) != 0) {
      printf("Error setting variable information.\n");
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) != 0) {
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
        // red-yellow gradient
        // note that Linux provides more precision (0-65535),
        // so we multiply ours (0-255) by 256
        r[i] = 255 << 8;
        g[i] = ((15 - i) * 16) << 8;
        b[i] = 0;
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
    screensize = finfo.smem_len; //vinfo.xres * vinfo.yres;
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
        sleep(1);

        // rotate palette to create visual effect
        int j;
        int fps = 30; // frames per second
        int d = 5; // duration in seconds
        // repeat for given time
        for(j = 0; j < fps * d; j++) {
            // store color 0 in temp variables
            int rt = r[0];
            int gt = g[0];
            int bt = b[0];
            // replace colors by copying the next
            for(i = 0; i < 15; i++) {
                r[i] = r[i+1];
                g[i] = g[i+1];
                b[i] = b[i+1];
            }
            // restore last one from temp
            r[15] = rt;
            g[15] = gt;
            b[15] = bt;
            // Note that we set up the 'pal' structure earlier
            // and it still points to the r, g, b arrays,
            // so we can just reuse 'pal' here
            if (ioctl(fbfd, FBIOPUTCMAP, &pal) != 0) {
                printf("Error setting palette.\n");
            }
            usleep(1000000 / fps);
        }

    }

    // cleanup
    // NOTE: should probably reset the palette too...
    // unmap fb file from memory
    munmap(fbp, screensize);
    // reset the display mode
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo) != 0) {
        printf("Error re-setting variable information.\n");
    }
    // close fb file
    close(fbfd);

    return 0;

}
