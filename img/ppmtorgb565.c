/*
 * ppmtorgb565.c
 *
 * Converts a 24 bit P6 PPM file to 'raw' RGB 16 bit 5:6:5 format
 *
 * To build:
 *   gcc -O2 -o ppmtorgb565 ppmtorgb565.c
 *
 * Usage:
 *   - make sure you have a 24 bit PNG to begin with and the image pixel dimensions
 *     match your target
 *   (- sudo apt-get install netpbm)
 *   - pngtopnm /path/to/image24.png | /path/to/ppmtorgb565 > /path/to/image565.raw
 * Maybe even:
 *   - pngtopnm /path/to/image24.png | /path/to/ppmtorgb565 > /dev/fb1
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
#include <errno.h>

int main(int argc, char* argv[]) {

	int errval = 0;
	FILE* fp = 0;

	if (argc < 2) {
		fp = stdin;
	}
	else {
		fp = fopen(argv[1], "r");
		if (fp == 0) {
			errval = errno;
			fprintf(stderr, "Error opening file %s (errno=%d).\n", errval);
			return errval;
		}
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
		}

		if (fscanf(fp, "%d\n", &depth) == 1) {
			//fprintf(stderr, "d=%d\n", depth);
		}
		else
		{
			fprintf(stderr, "Read depth failed.\n");
			depth = -1;
		}

		if (depth != 255) {
			fprintf(stderr, "Only 255 depth supported.\n");
			depth = -1;
		}

	}
	else {
		fprintf(stderr, "Not a P6 ppm.\n");
	}

	if ( (width > -1) && (height > -1) && (depth = -1) ) {
		// header read ok
		errval = 0;

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
					unsigned char c1 = (rgb565 & 0xFF00) >> 8;
					unsigned char c2 = (rgb565 & 0xFF);
					fwrite(&c2, 1, 1, stdout);
					fwrite(&c1, 1, 1, stdout);
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

	if (fp != stdin) {
		fclose(fp);
	}

	return errval;
}
