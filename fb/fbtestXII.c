/*
 * fbtestXII.c
 *
 * 
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
#include <linux/kd.h>
#include <stdint.h>
#include "vcio.h"
#include <time.h>

// 'global' variables to store screen info
int fbfd = 0;
char *fbp = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

int mboxfd = 0;
int page_size = 0;
int cur_page = 0;

static struct timespec timediff(struct timespec start, struct timespec end) {
  struct timespec temp;
  if ((end.tv_nsec - start.tv_nsec) < 0) {
    temp.tv_sec = end.tv_sec - start.tv_sec - 1;
    temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
  }
  else {
    temp.tv_sec = end.tv_sec - start.tv_sec;
    temp.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return temp;
}

// helper function to talk to the mailbox interface
static int mbox_property(void *buf)
{
   if (mboxfd < -1) return -1;
   
   int ret_val = ioctl(mboxfd, IOCTL_MBOX_PROPERTY, buf);

   if (ret_val < 0) {
      printf("ioctl_set_msg failed:%d\n", ret_val);
   }

   return ret_val;
}

// helper function to set the framebuffer virtual offset == pan
static unsigned set_fb_voffs(unsigned *x, unsigned *y)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x00048009; // set virtual offset
   p[i++] = 0x00000008; // buffer size
   p[i++] = 0x00000000; // request size
   p[i++] = *x; // value buffer
   p[i++] = *y; // value buffer 2

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(p);
   *x = p[5];
   *y = p[6];
   return p[1];
}

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

void clear_screen(int c) {
    memset(fbp + cur_page * page_size, c, page_size);
}

// helper function for drawing - no more need to go mess with
// the main function when just want to change what to draw...
void draw() {

    int i, x, y, w, h, dx, dy;
    struct timespec pt;
    struct timespec ct;
    struct timespec df;

    // start position (upper left)
    x = 0;
    y = 0;
    // rectangle dimensions
    w = vinfo.yres / 10;
    h = w;
    // move step 'size'
    dx = 1;
    dy = 1;

    int fps = 100;
    int secs = 10;
    
    int vx, vy;

    clock_gettime(CLOCK_REALTIME, &pt);
    
    // loop for a while
    for (i = 0; i < (fps * secs); i++) {

        // change page to draw to (between 0 and 1)
        cur_page = (cur_page + 1) % 2;
    
        // clear the previous image (= fill entire screen)
        clear_screen(0);
        
        // draw the bouncing rectangle
        fill_rect(x, y, w, h, 15);

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
        
        // switch page
        /*
        vinfo.yoffset = cur_page * vinfo.yres;
        vinfo.activate = FB_ACTIVATE_VBL;
        if (ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo)) {
            printf("Error panning display.\n");
        }
        */
        vx = 0;
        vy = cur_page * vinfo.yres;
        set_fb_voffs(&vx, &vy);
        
        //usleep(1000000 / fps);
    }

    clock_gettime(CLOCK_REALTIME, &ct);
    df = timediff(pt, ct);
    printf("done in %ld s %5ld ms\n", df.tv_sec, df.tv_nsec / 1000000);
}

// application entry point
int main(int argc, char* argv[])
{

    struct fb_var_screeninfo orig_vinfo;
    long int screensize = 0;


    // Open the framebuffer file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (!fbfd) {
      printf("Error: cannot open framebuffer device.\n");
      return(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // hide cursor
    char *kbfds = "/dev/tty";
    int kbfd = open(kbfds, O_WRONLY);
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_GRAPHICS);
    }
    else {
        printf("Could not open %s.\n", kbfds);
    }

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
    vinfo.xres = 480;
    vinfo.yres = 270;
    vinfo.xres_virtual = vinfo.xres;
    vinfo.yres_virtual = vinfo.yres * 2;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
      printf("Error setting variable information.\n");
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
      printf("Error reading fixed information.\n");
    }
    //printf("Fixed info: smem_len %d, line_length %d\n", finfo.smem_len, finfo.line_length);
    
    page_size = finfo.line_length * vinfo.yres;

    // map fb to user mem 
    screensize = finfo.smem_len;
    fbp = (char*)mmap(0, 
              screensize, 
              PROT_READ | PROT_WRITE, 
              MAP_SHARED, 
              fbfd, 
              0);

    // open a char device file used for communicating with kernel mbox driver
    mboxfd = open(DEVICE_FILE_NAME, 0);
    if (mboxfd < 0) {
        printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
        printf("Try creating a device file with: mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
        
    }

    if ((int)fbp == -1) {
        printf("Failed to mmap\n");
    }
    else if (mboxfd < 0) {
    }
    else {
        // draw...
        draw();
        //sleep(5);
    }

    // cleanup
    munmap(fbp, screensize);
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
        printf("Error re-setting variable information.\n");
    }
    close(fbfd);

    // reset cursor
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_TEXT);
    }

    return 0;
  
}
