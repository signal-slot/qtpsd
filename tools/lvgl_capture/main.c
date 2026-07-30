/**
 * Headless LVGL capture harness.
 *
 * Linked together with a generated main_screen.c (see the LVGL exporter),
 * renders the screen with the real LVGL engine, takes a snapshot,
 * and writes the result as a PNG file.
 *
 * Usage: capture <export_dir> <output.png>
 *
 * The export_dir must contain the images/ directory referenced by the
 * generated code ("A:images/..." paths resolve relative to it).
 */

#include "lvgl.h"
#include "src/libs/lodepng/lodepng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Provided by the generated main_screen.c */
extern const int32_t main_screen_width;
extern const int32_t main_screen_height;
lv_obj_t * main_screen_create(lv_obj_t * parent);

static uint32_t tick_get_cb(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Display buffer — filled by flush callback */
static uint32_t *framebuffer;
static int32_t disp_width;
static int32_t disp_height;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = area->x2 - area->x1 + 1;
    for (int32_t y = area->y1; y <= area->y2; y++) {
        memcpy(&framebuffer[y * disp_width + area->x1],
               px_map, w * sizeof(uint32_t));
        px_map += w * sizeof(uint32_t);
    }
    lv_display_flush_ready(disp);
}

static int write_png(const char *path, const uint32_t *pixels, int32_t w, int32_t h)
{
    /* Convert ARGB8888 (LVGL native on little-endian) to RGBA8888 for PNG */
    uint8_t *rgba = malloc(w * h * 4);
    if (!rgba) return -1;

    for (int32_t i = 0; i < w * h; i++) {
        uint32_t px = pixels[i];
        /* LVGL stores as 0xAARRGGBB in memory (ARGB8888) */
        rgba[i * 4 + 0] = (px >> 16) & 0xFF; /* R */
        rgba[i * 4 + 1] = (px >> 8) & 0xFF;  /* G */
        rgba[i * 4 + 2] = (px >> 0) & 0xFF;  /* B */
        rgba[i * 4 + 3] = (px >> 24) & 0xFF; /* A */
    }

    unsigned char *png_data = NULL;
    size_t png_size = 0;
    unsigned error = lodepng_encode32(&png_data, &png_size, rgba, w, h);
    free(rgba);

    if (error) {
        fprintf(stderr, "PNG encode error %u: %s\n", error, lodepng_error_text(error));
        return -1;
    }

    /* Write using stdlib to bypass LVGL filesystem */
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", path);
        lv_free(png_data);
        return -1;
    }
    fwrite(png_data, 1, png_size, f);
    fclose(f);
    lv_free(png_data);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <export_dir> <output.png>\n", argv[0]);
        return 1;
    }

    const char *export_dir = argv[1];
    const char *output_png = argv[2];

    int32_t width = main_screen_width;
    int32_t height = main_screen_height;
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid screen size %dx%d\n", (int)width, (int)height);
        return 1;
    }

    disp_width = width;
    disp_height = height;

    /* Change working directory so that "A:" FS driver resolves relative paths */
    if (chdir(export_dir) != 0) {
        fprintf(stderr, "Failed to chdir to %s\n", export_dir);
        return 1;
    }

    /* Initialize LVGL */
    lv_init();
    lv_tick_set_cb(tick_get_cb);

    /* Create display with buffer-based rendering (headless) */
    framebuffer = calloc(width * height, sizeof(uint32_t));
    if (!framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer\n");
        return 1;
    }

    lv_display_t *disp = lv_display_create(width, height);
    size_t buf_size = width * height * sizeof(lv_color32_t);
    void *draw_buf = malloc(buf_size);
    lv_display_set_buffers(disp, draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);

    /* Disable the default theme to prevent borders, padding etc. */
    lv_display_set_theme(disp, NULL);

    /* Set screen background to transparent */
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_TRANSP, 0);

    if (main_screen_create(lv_screen_active()) == NULL) {
        fprintf(stderr, "main_screen_create failed\n");
        free(framebuffer);
        free(draw_buf);
        return 1;
    }

    /* Run timer handler to render frames */
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
    }

    /* Write the framebuffer as PNG */
    if (write_png(output_png, framebuffer, width, height) != 0) {
        fprintf(stderr, "Failed to write PNG: %s\n", output_png);
        free(framebuffer);
        free(draw_buf);
        return 1;
    }

    free(framebuffer);
    free(draw_buf);
    return 0;
}
