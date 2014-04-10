/*
 * fbtestXX.c
 *
 * raspberrycompote.blogspot.com/2014/04/low-level-graphics-on-raspberry-pi.html
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

// helper function to draw a line in given color
// (uses Bresenham's line algorithm)
void draw_line(int x0, int y0, int x1, int y1, int c) {
  int dx = x1 - x0;
  dx = (dx >= 0) ? dx : -dx; // abs()
  int dy = y1 - y0;
  dy = (dy >= 0) ? dy : -dy; // abs()
  int sx;
  int sy;
  if (x0 < x1)
    sx = 1;
  else
    sx = -1;
  if (y0 < y1)
    sy = 1;
  else
    sy = -1;
  int err = dx - dy;
  int e2;
  int done = 0;
  while (!done) {
    put_pixel(x0, y0, c);
    if ((x0 == x1) && (y0 == y1))
      done = 1;
    else {
      e2 = 2 * err;
      if (e2 > -dy) {
        err = err - dy;
        x0 = x0 + sx;
      }
      if (e2 < dx) {
        err = err + dx;
        y0 = y0 + sy;
      }
    }
  }
}

// helper function to draw a rectangle outline in given color
void draw_rect(int x0, int y0, int w, int h, int c) {
  draw_line(x0, y0, x0 + w, y0, c); // top
  draw_line(x0, y0, x0, y0 + h, c); // left
  draw_line(x0, y0 + h, x0 + w, y0 + h, c); // bottom
  draw_line(x0 + w, y0, x0 + w, y0 + h, c); // right
}

// helper function to draw a rectangle outline in given color
void fill_rect(int x0, int y0, int w, int h, int c) {
  int y;
  for (y = 0; y < h; y++) {
    draw_line(x0, y0 + y, x0 + w, y0 + y, c);
  }
}

// helper function to draw a circle outline in given color
// (uses Bresenham's circle algorithm)
void draw_circle(int x0, int y0, int r, int c)
{
  int x = r;
  int y = 0;
  int radiusError = 1 - x;

  while(x >= y)
  {
    // top left
    put_pixel(-y + x0, -x + y0, c);
    // top right
    put_pixel(y + x0, -x + y0, c);
    // upper middle left
    put_pixel(-x + x0, -y + y0, c);
    // upper middle right
    put_pixel(x + x0, -y + y0, c);
    // lower middle left
    put_pixel(-x + x0, y + y0, c);
    // lower middle right
    put_pixel(x + x0, y + y0, c);
    // bottom left
    put_pixel(-y + x0, x + y0, c);
    // bottom right
    put_pixel(y + x0, x + y0, c);

    y++;
    if (radiusError < 0)
	{
	  radiusError += 2 * y + 1;
	} else {
      x--;
	  radiusError+= 2 * (y - x + 1);
    }
  }
}

// helper function to draw a filled circle in given color
// (uses Bresenham's circle algorithm)
void fill_circle(int x0, int y0, int r, int c) {
  int x = r;
  int y = 0;
  int radiusError = 1 - x;

  while(x >= y)
  {
    // top
    draw_line(-y + x0, -x + y0, y + x0, -x + y0, c);
    // upper middle
    draw_line(-x + x0, -y + y0, x + x0, -y + y0, c);
    // lower middle
    draw_line(-x + x0, y + y0, x + x0, y + y0, c);
    // bottom 
    draw_line(-y + x0, x + y0, y + x0, x + y0, c);

    y++;
    if (radiusError < 0)
    {
      radiusError += 2 * y + 1;
    } else {
      x--;
      radiusError+= 2 * (y - x + 1);
    }
  }
}


// helper function for drawing - no more need to go mess with
// the main function when just want to change what to draw...
void draw() {

  int x;
  
  // some pixels
  for (x = 0; x < vinfo.xres; x+=5) {
    put_pixel(x, vinfo.yres / 2, WHITE);
  }

  // some lines (note the quite likely 'Moire pattern')
  for (x = 0; x < vinfo.xres; x+=20) {
    draw_line(0, 0, x, vinfo.yres, GREEN);
  }
  
  // some rectangles
  draw_rect(vinfo.xres / 4, vinfo.yres / 2 + 10, vinfo.xres / 4, vinfo.yres / 4, PURPLE);  
  draw_rect(vinfo.xres / 4 + 10, vinfo.yres / 2 + 20, vinfo.xres / 4 - 20, vinfo.yres / 4 - 20, PURPLE);  
  fill_rect(vinfo.xres / 4 + 20, vinfo.yres / 2 + 30, vinfo.xres / 4 - 40, vinfo.yres / 4 - 40, YELLOW);  

  // some circles
  int d;
  for(d = 10; d < vinfo.yres / 6; d+=10) {
    draw_circle(3 * vinfo.xres / 4, vinfo.yres / 4, d, RED);
  }
  
  fill_circle(3 * vinfo.xres / 4, 3 * vinfo.yres / 4, vinfo.yres / 6, ORANGE);
  fill_circle(3 * vinfo.xres / 4, 3 * vinfo.yres / 4, vinfo.yres / 8, RED);

}


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
  printf("The framebuffer device was opened successfully.\n");

  // Get variable screen information
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
    printf("Error reading variable information.\n");
  }
  printf("Original %dx%d, %dbpp\n", vinfo.xres, vinfo.yres,
	 vinfo.bits_per_pixel );

  // Store for reset (copy vinfo to vinfo_orig)
  memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

  // Change variable info
  vinfo.bits_per_pixel = 8;
  if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo) == -1) {
    printf("Error setting variable information.\n");
  }

  // Get fixed screen information
  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
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
    draw();
    sleep(5);
  }

  // cleanup
  munmap(fbp, screensize);
  if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo) == -1) {
    printf("Error re-setting variable information.\n");
  }
  close(fbfd);

  return 0;
}
