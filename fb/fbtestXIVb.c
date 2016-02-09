/*
 * fbtestXIVb.c
 *
 * compile with 'gcc -O2 -o fbtestXIVb fbtestXIVb.c'
 * run with './fbtestXIVb'
 *
 * http://raspberrycompote.blogspot.com/2015/01/low-level-graphics-on-raspberry-pi-part.html
 * http://raspberrycompote.blogspot.com/2015/01/low-level-graphics-on-raspberry-pi-part_27.html
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
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/ioctl.h>

// 'global' variables to store screen info
int fbfd = 0;
char *fbp = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

int page_size = 0;
int cur_page = 0;

#define NUM_ELEMS 200
int xs[NUM_ELEMS];
int ys[NUM_ELEMS];
int dxs[NUM_ELEMS];
int dys[NUM_ELEMS];

// helper function to 'plot' a pixel in given color
void put_pixel(int x, int y, int c)
{
    // calculate the pixel's byte offset inside the buffer
    unsigned int pix_offset = x + y * finfo.line_length;

    // offset by the current buffer start
    pix_offset += cur_page * page_size;

    // now this is about the same as 'fbp[pix_offset] = value'
    *((char*)(fbp + pix_offset)) = c;

}

// helper function to draw a rectangle in given color
void fill_rect(int x, int y, int w, int h, int c) {
    int cx, cy;
    for (cy = 0; cy < h; cy++) {
        for (cx = 0; cx < w; cx++) {
            put_pixel(x + cx, y + cy, c);
        }
    }
}

// helper to clear (fill with given color) the screen
void clear_screen(int c) {
    memset(fbp + cur_page * page_size, c, page_size);
}

// helper function for drawing - no more need to go mess with
// the main function when just want to change what to draw...
void draw() {

    int i, x, y, w, h, dx, dy;

    // rectangle dimensions
    w = vinfo.yres / 10;
    h = w;

    // start position (upper left)
    x = 0;
    y = 0;
    int n;
    for (n = 0; n < NUM_ELEMS; n++) {
        int ex = rand() % (vinfo.xres - w);
        int ey = rand() % (vinfo.yres - h);
        xs[n] = ex;
        ys[n] = ey;
        int edx = (rand() % 10) + 1;
        int edy = (rand() % 10) + 1;
        dxs[n] = edx;
        dys[n] = edy;
    }

    // move step 'size'
    dx = 1;
    dy = 1;

    int fps = 60;
    int secs = 10;
    int vx, vy;

    // loop for a while
    for (i = 0; i < (fps * secs); i++) {

        // change page to draw to (between 0 and 1)
        cur_page = (cur_page + 1) % 2;

        // clear the previous image (= fill entire screen)
        clear_screen(0);

        for (n = 0; n < NUM_ELEMS; n++) {
            x = xs[n];
            y = ys[n];
            dx = dxs[n];
            dy = dys[n];

            // draw the bouncing rectangle
            fill_rect(x, y, w, h, (n % 15) + 1);

            // move the rectangle
            x = x + dx;
            y = y + dy;

            // check for display sides
            if ((x < 0) || (x > (vinfo.xres - w))) {
                dx = -dx; // reverse direction
                x = x + 2 * dx; // counteract the move already done above
            }
            // same for vertical dir
            if ((y < 0) || (y > (vinfo.yres - h))) {
                dy = -dy;
                y = y + 2 * dy;
            }

            xs[n] = x;
            ys[n] = y;
            dxs[n] = dx;
            dys[n] = dy;
        }

        // switch page
        vinfo.yoffset = cur_page * vinfo.yres;
        __u32 dummy = 0;
        ioctl(fbfd, FBIO_WAITFORVSYNC, &dummy);
        // would expect this order to work but tearing occurs...
        ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);

    }

}

// application entry point
int main(int argc, char* argv[])
{

    struct fb_var_screeninfo orig_vinfo;
    long int screensize = 0;

    // Open the framebuffer file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
      printf("Error: cannot open framebuffer device.\n");
      return(1);
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
      printf("Error reading variable information.\n");
    }

    // Store for reset (copy vinfo to vinfo_orig)
    memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

    // Change variable info
    vinfo.bits_per_pixel = 8;
    vinfo.xres = 960;
    vinfo.yres = 540;
    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres * 2;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
      printf("Error setting variable information.\n");
    }

    // hide cursor
    char *kbfds = "/dev/tty";
    int kbfd = open(kbfds, O_WRONLY);
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_GRAPHICS);
    }
    else {
        printf("Could not open %s.\n", kbfds);
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
      printf("Error reading fixed information.\n");
    }

    page_size = finfo.line_length * vinfo.yres;

    // map fb to user mem
    screensize = finfo.smem_len;
    fbp = (char*)mmap(0,
              screensize,
              PROT_READ | PROT_WRITE,
              MAP_SHARED,
              fbfd,
              0);

    if ((int)fbp == -1) {
        printf("Failed to mmap\n");
    }
    else {
        // draw...
        draw();
        //sleep(5);
    }

    // unmap fb file from memory
    munmap(fbp, screensize);
    // reset cursor
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_TEXT);
        // close kb file
        close(kbfd);
    }
    // reset the display mode
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
        printf("Error re-setting variable information.\n");
    }
    // close fb file    
    close(fbfd);

    return 0;

}
