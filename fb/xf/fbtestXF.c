/*
 * fbtestXF.c
 *
 * Cross-fade test (requires two 24bit raw files same size as the display...)
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

// 'global' variables to store screen info
char *fbp = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

char *img1 = 0;
char *img2 = 0;

void put_pixel_RGB24(int x, int y, int r, int g, int b)
{
    unsigned int pix_offset = x * 3 + y * finfo.line_length;

    *((char*)(fbp + pix_offset)) = r;
    *((char*)(fbp + pix_offset + 1)) = g;
    *((char*)(fbp + pix_offset + 2)) = b;

}

void draw() {
    
    int x, y, r, g, b, r2, g2, b2, ofs;
    
    // draw image1
    for (y = 0; y < vinfo.yres; y++) {
        ofs = vinfo.xres * y * 3;
        for (x = 0; x < vinfo.xres; x++) {
            r = *((char *)(img1 + ofs + x * 3));
            g = *((char *)(img1 + ofs + x * 3 + 1));
            b = *((char *)(img1 + ofs + x * 3 + 2));
            put_pixel_RGB24(x, y, r, g, b);
        }
    }

    sleep(2);
    
    // cross-fade to image2
    int fadesteps = 25;
    int n;
    for (n = 0; n < fadesteps; n++) {
        for (y = 0; y < vinfo.yres; y++) {
            ofs = vinfo.xres * y * 3;
            for (x = 0; x < vinfo.xres; x++) {
                r = *((char *)(img1 + ofs + x * 3));
                g = *((char *)(img1 + ofs + x * 3 + 1));
                b = *((char *)(img1 + ofs + x * 3 + 2));
                r2 = *((char *)(img2 + ofs + x * 3));
                g2 = *((char *)(img2 + ofs + x * 3 + 1));
                b2 = *((char *)(img2 + ofs + x * 3 + 2));
                put_pixel_RGB24(x, y, r + n * (r2 - r) / fadesteps, 
                                      g + n * (g2 - g) / fadesteps, 
                                      b + n * (b2 - b) / fadesteps);
            }
        }
    }
    
    sleep(5);
    
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
    vinfo.bits_per_pixel = 24;
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
      printf("Error setting variable information.\n");
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
      printf("Error reading fixed information.\n");
    }
    printf("Fixed info: smem_len %d, line_length %d\n", finfo.smem_len, finfo.line_length);

    // map fb to user mem 
    screensize = finfo.smem_len;
    fbp = (char*)mmap(0, 
              screensize, 
              PROT_READ | PROT_WRITE, 
              MAP_SHARED, 
              fbfd, 
              0);

    img1 = malloc(screensize);
    img2 = malloc(screensize);
              
    if ((int)fbp == -1) {
        printf("Failed to mmap.\n");
    }
    else {
        if ((img1 == 0) || (img2 ==0)) {
            printf("Failed to malloc.\n");
        }
        else {
            //memset(img1, 0xFF, screensize);
            FILE * ifp = fopen("img1.raw", "r");
            int nr = fread(img1, screensize, 1, ifp);
            fclose(ifp);
            //memset(img2, 0, screensize);
            /*
            int i;
            for (i = 0; i < screensize; i++) {
                *((char *)(img2 + i)) = i % 256;
            }
            */
            ifp = fopen("img2.raw", "r");
            fread(img2, screensize, 1, ifp);
            fclose(ifp);
            // draw...
            draw();
            //sleep(5);
        }
    }

    // cleanup
    free(img1);
    free(img2);
    munmap(fbp, screensize);
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) {
        printf("Error re-setting variable information.\n");
    }
    close(fbfd);

    return 0;
  
}
