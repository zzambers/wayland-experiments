#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <png.h>
// #include <X11/extensions/Xcomposite.h>

typedef struct {
    Display *display;
    Window rootWindow;
    int x, y, w, h;
    char *rgbData;
    XImage *image;
    Window *children;
    FILE *fp;
    png_byte **row_ptrs;
    png_structp png_ptr;
    png_infop info_ptr;
} prog_data;


void cleanup(prog_data *pdata) {
    if (pdata->png_ptr) {
        png_destroy_write_struct(&pdata->png_ptr, pdata->info_ptr == NULL ? NULL : &pdata->info_ptr);
    }
    if (pdata->row_ptrs) {
        free(pdata->row_ptrs);
    }
    if (pdata->fp) {
        fclose(pdata->fp);
    }
    if (pdata->image) {
        XFree(pdata->image);
    }
    if (pdata->children) {
        XFree(pdata->children);
    }
    if (pdata->display) {
        XCloseDisplay(pdata->display);
    }
}

void printWindowAttributes(XWindowAttributes *attributes) {
    fprintf(stdout, "x: %d, y: %d\n", attributes->x, attributes->y);
    fprintf(stdout, "width: %d, height: %d\n", attributes->width, attributes->height);
    fprintf(stdout, "depth: %d\n", attributes->depth);
    fprintf(stdout, "visual: %p\n", attributes->visual);
    fprintf(stdout, "root: %d\n", attributes->root);
    fprintf(stdout, "class: %d\n", attributes->class);
    fprintf(stdout, "bit_gravity: %d\n", attributes->bit_gravity);
    fprintf(stdout, "win_gravity: %d\n", attributes->win_gravity);
    fprintf(stdout, "backing_store: %d\n", attributes->backing_store);
    fprintf(stdout, "backing_planes: %lu\n", attributes->backing_planes);
    fprintf(stdout, "backing_pixel: %lu\n", attributes->backing_pixel);
    fprintf(stdout, "save_under: %d\n", attributes->save_under);
    fprintf(stdout, "colormap: %d\n", attributes->colormap);
    fprintf(stdout, "map_installed: %d\n", attributes->map_installed);
    fprintf(stdout, "map_state: %d\n", attributes->map_state);
    fprintf(stdout, "all_event_masks: %ld\n", attributes->all_event_masks);
    fprintf(stdout, "your_event_mask: %ld\n", attributes->your_event_mask);
    fprintf(stdout, "do_not_propagate_mask: %ld\n", attributes->do_not_propagate_mask);
    fprintf(stdout, "override_redirect: %d\n", attributes->override_redirect);
    fprintf(stdout, "screen: %p\n", attributes->screen);
}

int minmax(int i, int min, int max) {
    return i < min ? min :
        i > max ? max :
        i;
}


void getImageData(char *rgbData, int dw, int dh, XImage *image, int x, int y, int w, int h) {
    int offset;
    long pixel;
    int cx, cy, x1, y1, x2, y2;
    x1 = minmax(x, 0, dw);
    y1 = minmax(y, 0, dh);
    x2 = minmax(x + w, 0, dw);
    y2 = minmax(y + h, 0, dh);
    for (cy = y1; cy < y2; ++cy) {
        for (cx = x1; cx < x2; ++cx) {
            pixel = XGetPixel(image, cx - x, cy - y);
            offset = 3 * (cy * dw + cx);
            rgbData[offset] = (char) ((pixel >> 16) & 0xFF);
            rgbData[offset + 1] = (char) ((pixel >> 8) & 0xFF);
            rgbData[offset + 2] = (char) (pixel & 0xFF);
        }
    }
}


int takeScreenshot(prog_data *pdata) {
    Window rootRet;
    Window parentRet;
    Window *childrenRet;
    unsigned int nchildren, i;
    XWindowAttributes attributes = {0};
    int x, y, w, h;

    /*
    pdata->image = XGetImage(pdata->display, pdata->rootWindow, 0, 0, pdata->w, pdata->h, AllPlanes, ZPixmap);
    if (pdata->image == NULL) {
        fprintf(stderr, "%s\n", "XGetImage failed");
        return -1;
    }
    getImageData(pdata->rgbData, pdata->w, pdata->h, pdata->image, 0, 0, pdata->w, pdata->h);
    */
    if (!XQueryTree(pdata->display, pdata->rootWindow, &rootRet, &parentRet, &childrenRet, &nchildren)) {
        fprintf(stderr, "%s\n", "XQueryTree failed");
        return -1;
    }
    for (i = 0; i < nchildren; ++i) {
        fprintf(stderr, "%s: %d\n", "Child window", pdata->children[i]);
        if (!XGetWindowAttributes(pdata->display, pdata->children[i], &attributes)) {
            fprintf(stderr, "%s\n", "XGetWindowAttributes failed");
            return -1;
        }
        if (attributes.class == InputOutput && attributes.map_state == IsViewable) {
            printWindowAttributes(&attributes);
            fprintf(stdout, "\n");
            // clip region by root window
            x = minmax(attributes.x, 0, pdata->w);
            y = minmax(attributes.y, 0, pdata->h);
            w = minmax(attributes.width + (attributes.x - x), 0, pdata->w - x);
            h = minmax(attributes.height + (attributes.y - y), 0, pdata->h - y);
            if (w > 0 && h > 0) {
                pdata->image = XGetImage(pdata->display, pdata->children[i], x - attributes.x, y - attributes.y, w, h, AllPlanes, ZPixmap);
                if (pdata->image == NULL) {
                    fprintf(stderr, "%s\n", "XGetImage (loop) failed");
                    return -1;
                }
                getImageData(pdata->rgbData, pdata->w, pdata->h, pdata->image, x, y, w, h);
                XFree(pdata->image);
                pdata->image = NULL;
            }
        }
    }
    return 0;
}


int saveScreenshot(prog_data *pdata) {
    /* http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-4 */
    int y;
    fprintf(stderr, "%s\n", "screenshot");
    pdata->fp = fopen("./Screenshot.png", "wb");
    pdata->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!pdata->png_ptr) {
        fprintf(stderr, "%s\n", "png_create_write_struct failed");
        return -1;
    }
    pdata->info_ptr = png_create_info_struct(pdata->png_ptr);
    if (!pdata->info_ptr) {
        fprintf(stderr, "%s\n", "png_create_info_struct failed");
        return -1;
    }
    if (setjmp(png_jmpbuf(pdata->png_ptr))) {
        fprintf(stderr, "%s\n", "libpng error");
        return -1;
    }
    png_set_IHDR(pdata->png_ptr, pdata->info_ptr,
            (png_uint_32) pdata->w, (png_uint_32) pdata->h,
            (png_byte) 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    pdata->row_ptrs = calloc(pdata->h, pdata->w * sizeof(png_byte *));
    if (!pdata->row_ptrs) {
        fprintf(stderr, "%s\n", "failed to alloc row ptrs");
        return -1;
    }
    for (y = 0; y < pdata->h; ++y) {
        pdata->row_ptrs[y] = pdata->rgbData + y * pdata->w * 3;
    }
    png_init_io(pdata->png_ptr, pdata->fp);
    png_set_rows(pdata->png_ptr, pdata->info_ptr, pdata->row_ptrs);
    png_write_png(pdata->png_ptr, pdata->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    return 0;
}


int main2(prog_data *pdata) {
    XWindowAttributes attributes = {0};
    XImage *image = NULL;
    pdata->display = XOpenDisplay(NULL);
    if (pdata->display == NULL) {
        fprintf(stderr, "%s\n", "XOpenDisplay failed");
        return -1;
    }
    pdata->rootWindow = XDefaultRootWindow(pdata->display);
    if (pdata->rootWindow == 0) {
        fprintf(stderr, "%s\n", "XDefaultRootWindow failed");
        return -1;
    }
    if (!XGetWindowAttributes(pdata->display, pdata->rootWindow, &attributes)) {
        fprintf(stderr, "%s\n", "XGetWindowAttributes failed");
        return -1;
    }
    /*
    pdata->rootWindow = XCompositeGetOverlayWindow(pdata->display, pdata->rootWindow);
    if (pdata->rootWindow == 0) {
        fprintf(stderr, "%s\n", "XCompositeGetOverlayWindow failed");
        return -1;
    }
    */
    // printWindowAttributes(&attributes);
    pdata->w = attributes.width;
    pdata->h = attributes.height;
    pdata->rgbData = calloc(1, 3 * pdata->w * pdata->h);
    if (pdata->rgbData == NULL) {
        fprintf(stderr, "%s\n", "Failed to allocate rgb data");
        return -1;
    }
    if (takeScreenshot(pdata)) {
        return -1;
    }
    if (saveScreenshot(pdata)) {
        return -1;
    }
    return 0;
}


int main(int argc, char **argv) {
    prog_data data = {0};
    int ret = main2(&data);
    cleanup(&data);
    return ret;
}
