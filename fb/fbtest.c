/*
 * fbtest.c
 *
 * http://raspberrycompote.blogspot.com/2012/12/low-level-graphics-on-raspberry-pi-part_9509.html
 * http://raspberrycompote.blogspot.com/2016/03/low-level-graphics-on-raspberry-pi-vs.html
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
    int fbfd = 0; // framebuffer filedescriptor
    struct fb_var_screeninfo var_info;
    struct fb_fix_screeninfo fix_info;

    // Open the framebuffer device file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        printf("Error: cannot open framebuffer device.\n");
        return(1);
    }
    printf("The framebuffer device opened.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fix_info)) {
        printf("Error reading fixed screen info.\n");
    }
    printf("Fixed info:\n");
    printf(" id = %s\n type=%d\n accel=%d\n",
                fix_info.id,
                fix_info.type,
                fix_info.accel
	);
    printf(" smem_len=%d\n line_length=%d\n capabs=%d\n",
		fix_info.smem_len,
                fix_info.line_length,
                fix_info.capabilities
	);

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &var_info)) {
        printf("Error reading variable screen info.\n");
    }
    printf("Variable info:\n %dx%d, %d bpp\n",
                 var_info.xres, var_info.yres,
                 var_info.bits_per_pixel );

    // close fb file
    close(fbfd);

    return 0;

}
