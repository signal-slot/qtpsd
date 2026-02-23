/**
 * Headless LVGL XML capture tool.
 *
 * Loads an LVGL XML component (MainScreen.xml + globals.xml),
 * renders it using the real LVGL engine, takes a snapshot,
 * and writes the result as a PNG file.
 *
 * Usage: lvgl_capture <export_dir> <output.png>
 *
 * The export_dir must contain MainScreen.xml, globals.xml, and images/.
 */

#include "lvgl.h"
#include "src/libs/lodepng/lodepng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static int parse_view_size(const char *xml_path, int32_t *w, int32_t *h)
{
    /* Quick parse: read the file and find width="..." height="..." in the <view> tag */
    FILE *f = fopen(xml_path, "r");
    if (!f) return -1;

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* Find the view element */
    char *view = strstr(buf, "<view ");
    if (!view) view = strstr(buf, "<view\n");
    if (!view) return -1;

    char *width_attr = strstr(view, "width=\"");
    char *height_attr = strstr(view, "height=\"");
    if (!width_attr || !height_attr) return -1;

    *w = atoi(width_attr + 7);
    *h = atoi(height_attr + 8);
    return (*w > 0 && *h > 0) ? 0 : -1;
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

    /* Build paths */
    char main_xml[1024], globals_xml[1024];
    snprintf(main_xml, sizeof(main_xml), "%s/MainScreen.xml", export_dir);
    snprintf(globals_xml, sizeof(globals_xml), "%s/globals.xml", export_dir);

    /* Parse view dimensions from MainScreen.xml */
    int32_t width = 0, height = 0;
    if (parse_view_size(main_xml, &width, &height) != 0) {
        fprintf(stderr, "Failed to parse view size from %s\n", main_xml);
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

    /* Register globals.xml (images, gradients, styles)
     * Use "A:./" prefix so lv_fs_get_last() finds the '/' separator
     * and correctly extracts the filename (e.g. "globals" not "A:globals") */
    if (access(globals_xml, F_OK) == 0) {
        lv_result_t res = lv_xml_component_register_from_file("A:./globals.xml");
        if (res != LV_RESULT_OK) {
            fprintf(stderr, "Warning: failed to register globals.xml\n");
        }
    }

    /* Register MainScreen component */
    lv_result_t res = lv_xml_component_register_from_file("A:./MainScreen.xml");
    if (res != LV_RESULT_OK) {
        fprintf(stderr, "Failed to register MainScreen.xml\n");
        free(framebuffer);
        free(draw_buf);
        return 1;
    }

    /* Create the component */
    lv_obj_t *screen = (lv_obj_t *)lv_xml_create(lv_screen_active(), "MainScreen", NULL);
    if (!screen) {
        fprintf(stderr, "Failed to create MainScreen component\n");
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
