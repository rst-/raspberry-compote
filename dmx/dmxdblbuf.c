
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>

#include "bcm_host.h"

#define WIDTH   200
#define HEIGHT  200

#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))

typedef struct
{
    DISPMANX_DISPLAY_HANDLE_T   display;
    DISPMANX_MODEINFO_T         info;
    void                       *image;
    DISPMANX_UPDATE_HANDLE_T    update;
    DISPMANX_RESOURCE_HANDLE_T  resource0;
    DISPMANX_RESOURCE_HANDLE_T  resource1;
    uint32_t                    vc_image_ptr0;
    uint32_t                    vc_image_ptr1;
    DISPMANX_ELEMENT_HANDLE_T   element;

} RECT_VARS_T;

static RECT_VARS_T  gRectVars;

static void FillRect( VC_IMAGE_TYPE_T type, void *image, int pitch, int aligned_height, int x, int y, int w, int h, int val )
{
    int         row;
    int         col;

    uint16_t *line = (uint16_t *)image + y * (pitch>>1) + x;

    for ( row = 0; row < h; row++ )
    {
        for ( col = 0; col < w; col++ )
        {
            line[col] = val;
        }
        line += (pitch>>1);
    }
}

int main(void)
{
    RECT_VARS_T    *vars;
    uint32_t        screen = 0;
    int             ret;
    VC_RECT_T       src_rect;
	// separate destination rectangles for the resource and for the element
    VC_RECT_T       dst_rect_res;
    VC_RECT_T       dst_rect_elem;
    VC_IMAGE_TYPE_T type = VC_IMAGE_RGB565;
    int width = WIDTH, height = HEIGHT;
    int pitch = ALIGN_UP(width*2, 32);
    int aligned_height = ALIGN_UP(height, 16);
    VC_DISPMANX_ALPHA_T alpha = { 
		DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 
        255, /* fully opaque */
        0 
	};

    vars = &gRectVars;

    bcm_host_init();

    printf("Open display[%i]...\n", screen );
    vars->display = vc_dispmanx_display_open( screen );

    ret = vc_dispmanx_display_get_info( vars->display, &vars->info);
    assert(ret == 0);
    printf( "Display is %d x %d\n", vars->info.width, vars->info.height );

	// create the 'off-screen'/'in CPU accessible RAM' image buffer
    vars->image = calloc( 1, pitch * height );
    assert(vars->image);

	// create the two GPU resources for the 'double-buffer buffers'
    vars->resource0 = vc_dispmanx_resource_create( type,
                                                  width,
                                                  height,
                                                  &vars->vc_image_ptr0 );
    assert( vars->resource0 );
    vars->resource1 = vc_dispmanx_resource_create( type,
                                                  width,
                                                  height,
                                                  &vars->vc_image_ptr1 );
    assert( vars->resource1 );

	// initialise the rectangle structures
    vc_dispmanx_rect_set( &dst_rect_res, 0, 0, width, height);
    vc_dispmanx_rect_set( &src_rect, 0, 0, width << 16, height << 16 );
    vc_dispmanx_rect_set( &dst_rect_elem, 
						  ( vars->info.width - width ) / 2,
                          ( vars->info.height - height ) / 2,
                          width,
                          height );

	// initial blank image (cover full image with a black rectangle)
	FillRect( type, vars->image, pitch, aligned_height, 0, 0, width, height, 0x0 );

	// copy ('blit') the image to the first GPU buffer resource
	ret = vc_dispmanx_resource_write_data(  vars->resource0,
											type,
											pitch,
											vars->image,
											&dst_rect_res );
	assert( ret == 0 );

	// begin display update
	vars->update = vc_dispmanx_update_start( 10 );
	assert( vars->update );

	// create the 'window' element - based on the first buffer resource
	vars->element = vc_dispmanx_element_add(    vars->update,
												vars->display,
												2000,               // layer
												&dst_rect_elem,
												vars->resource0,
												&src_rect,
												DISPMANX_PROTECTION_NONE,
												&alpha,
												NULL,             // clamp
												VC_IMAGE_ROT0 );

	// finish display update
	ret = vc_dispmanx_update_submit_sync( vars->update );
	assert( ret == 0 );
	
	// these are used for switching between the buffers
    DISPMANX_RESOURCE_HANDLE_T cur_res = vars->resource1;
    DISPMANX_RESOURCE_HANDLE_T prev_res = vars->resource0;
	DISPMANX_RESOURCE_HANDLE_T tmp_res;
	
	// 'animation loop'
	int i, j;
	int maxd = 100;
	int step = 1;
	int maxstep = maxd / step;
	for (j = 0; j < 4; j++) {
	  for (i = 0; i < maxstep; i += step) {
	
		// clear image (fill with black)
		FillRect( type, vars->image, pitch, aligned_height, 0, 0, width, height, 0x0 );
		// for every second round of j ...
		if (j % 2 == 0) {
		  // draw a shrinking rectangle
		  FillRect( type, vars->image, pitch, aligned_height, i * step, i * step, width - 2 * i * step, height - 2 * i * step, 0xF800 );
		}
		else {
		  // draw a growing rectangle
		  FillRect( type, vars->image, pitch, aligned_height, maxd - i * step, maxd - i * step, 2 * i * step, 2 * i * step, 0xF800 );
		}

		// blit image to the current resource
		ret = vc_dispmanx_resource_write_data(  cur_res,
												type,
												pitch,
												vars->image,
												&dst_rect_res );
		assert( ret == 0 );

		// begin display update
		vars->update = vc_dispmanx_update_start( 10 );
		assert( vars->update );

		// change element source to be the current resource
		vc_dispmanx_element_change_source( vars->update, vars->element, cur_res );		
		
		// finish display update
		ret = vc_dispmanx_update_submit_sync( vars->update );
		assert( ret == 0 );
		
		// swap current resource
		tmp_res = cur_res;
		cur_res = prev_res;
		prev_res = tmp_res;
		
		// wait for a while
		usleep( 1000000 / 60 );
	  }
	}
	
    printf( "Done.\n" );

	// cleanup
    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );
    ret = vc_dispmanx_element_remove( vars->update, vars->element );
    assert( ret == 0 );
    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );
    ret = vc_dispmanx_resource_delete( vars->resource0 );
    assert( ret == 0 );
    ret = vc_dispmanx_display_close( vars->display );
    assert( ret == 0 );

    return 0;
}

