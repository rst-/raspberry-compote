/*
 * fbfire.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

// default framebuffer palette
typedef enum {
    BLACK        =  0, 
    BLUE         =  1, 
    GREEN        =  2, 
    CYAN         =  3, 
    RED          =  4, 
    PURPLE       =  5, 
    ORANGE       =  6, 
    LTGREY       =  7, 
    GREY         =  8, 
    LIGHT_BLUE   =  9, 
    LIGHT_GREEN  = 10, 
    LIGHT_CYAN   = 11, 
    LIGHT_RED    = 12, 
    LIGHT_PURPLE = 13, 
    YELLOW       = 14,
    WHITE        = 15 
} COLOR_INDEX_T;
//                             0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
unsigned short def_red[] =   { 0,   0,   0,   0, 128, 128, 128, 192, 128, 128, 128, 128, 255, 255, 128, 255};
unsigned short def_green[] = { 0,   0, 128, 128,   0,   0,  64, 192, 128, 128, 255, 255, 128, 128, 128, 255};
unsigned short def_blue[] =  { 0, 128,   0, 128,   0, 128,   0, 192, 128, 255, 128, 255, 128, 255,   0, 255};

// utility function to draw a pixel
void put_pixel(int x, int y, void *fbp, struct fb_var_screeninfo *vinfo, char c) {
    unsigned long offset = x + y * vinfo->xres;
    *((char*)(fbp + offset)) = c;
}

// utility function to get a pixel
char get_pixel(int x, int y, void *fbp, struct fb_var_screeninfo *vinfo) {
    unsigned long offset = x + y * vinfo->xres;
    return *((char*)(fbp + offset));
}

// application entry point
int main(int argc, char* argv[])
{
    int fbfd = 0; // framebuffer filedescriptor
    struct fb_var_screeninfo orig_var_info;
    struct fb_var_screeninfo var_info;
    struct fb_fix_screeninfo fix_info;
    char *fbp = 0; // framebuffer memory pointer

    // Open the framebuffer device file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (!fbfd) {
        printf("Error: cannot open framebuffer device.\n");
        return(1);
    }
    //printf("The framebuffer device opened.\n");

    // hide cursor
    char *kbfds = "/dev/tty";
    int kbfd = open(kbfds, O_WRONLY);
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_GRAPHICS);
    }
    else {
        printf("Could not open %s.\n", kbfds);
    }

    // Get original variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &var_info)) {
        printf("Error reading variable screen info.\n");
    }
    //printf("Original info %dx%d, %dbpp\n", var_info.xres, var_info.yres, var_info.bits_per_pixel );

    // Store for resetting before exit
    memcpy(&orig_var_info, &var_info, sizeof(struct fb_var_screeninfo));

    // Set variable info
    var_info.xres = 320; //480; //320;
    var_info.yres = 240; //280; //240;
    var_info.xres_virtual = var_info.xres;
    var_info.yres_virtual = var_info.yres;
    var_info.bits_per_pixel = 8;
    var_info.xoffset = 0;
    var_info.yoffset = 0;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &var_info)) {
        printf("Error setting variable screen info.\n");
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fix_info)) {
        printf("Error reading fixed screen info.\n");
    }

    // Create palette
    unsigned short r[256]; // red
    unsigned short g[256]; // green
    unsigned short b[256]; // blue
    int i;
    for (i = 0; i < 256; i++) {
        if (i < 32) {
            r[i] = i * 7 << 8;
            g[i] = b[i] = 0;
        }
        else if (i < 64) {
            r[i] = 224 << 8;
            g[i] = (i - 32) * 4 << 8;
            b[i] = 0;
        }
        else if (i < 96) {
            r[i] = 224 + (i - 64) << 8;
            g[i] = 128 + (i - 64) * 3 << 8;
            b[i] = 0;
        }
        else {
            r[i] = g[i] = 255 << 8;
            b[i] = 128 << 8;
        }
    }
    struct fb_cmap palette;
    palette.start = 0;
    palette.len = 256;
    palette.red = r;
    palette.green = g;
    palette.blue = b;
    palette.transp = 0; // null == no transparency settings
    // Set palette
    if (ioctl(fbfd, FBIOPUTCMAP, &palette)) {
        printf("Error setting palette.\n");
    }

    // map fb to user mem 
    long int screensize = fix_info.smem_len;
    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        printf("Failed to mmap.\n");
    }
  
    if ((int)fbp != -1) {
        // Draw
        int maxx = var_info.xres - 1;
        int maxy = var_info.yres - 1;
        int n = 0, r, c, x, y;
        int c0, c1, c2;
        while (n++ < 200) {

            // seed
            for (x = 1; x < maxx; x++) {

                r = rand();
                c = (r % 4 == 0) ? 192 : 32;
                put_pixel(x, maxy, fbp, &var_info, c);
                if ((r % 4 == 0)) { // && (r % 3 == 0)) {
                    c = 2 * c / 3;
                    put_pixel(x - 1, maxy, fbp, &var_info, c);
                    put_pixel(x + 1, maxy, fbp, &var_info, c);
                }
            }

            // smooth
            for (y = 1; y < maxy - 1; y++) {
                for (x = 1; x < maxx; x++) {
                    c0 = get_pixel(x - 1, y, fbp, &var_info);
                    c1 = get_pixel(x, y + 1, fbp, &var_info);
                    c2 = get_pixel(x + 1, y, fbp, &var_info);
                    c = (c0 + c1 + c1 + c2) / 4;
                    put_pixel(x, y - 1, fbp, &var_info, c);
                }
            }

            // convect
            for (y = 0; y < maxy; y++) {
                for (x = 1; x < maxx; x++) {
                    c = get_pixel(x, y + 1, fbp, &var_info);
                    if (c > 0) c--;
                    put_pixel(x, y, fbp, &var_info, c);
                }
            }

            //usleep(100);
        }
    }

    // Cleanup
    // unmap the file from memory
    munmap(fbp, screensize);
    // reset palette
    palette.start = 0;
    palette.len = 16;
    palette.red = def_red;
    palette.green = def_green;
    palette.blue = def_blue;
    palette.transp = 0; // null == no transparency settings
    if (ioctl(fbfd, FBIOPUTCMAP, &palette)) {
        printf("Error setting palette.\n");
    }
    // reset the display mode
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_var_info)) {
        printf("Error re-setting variable screen info.\n");
    }
    // close file  
    close(fbfd);
    // reset cursor
    if (kbfd >= 0) {
        ioctl(kbfd, KDSETMODE, KD_TEXT);
    }

    return 0;
}
