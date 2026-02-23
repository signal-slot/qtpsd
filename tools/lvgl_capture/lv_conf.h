/**
 * LVGL configuration for the headless XML capture tool.
 * Only enables features needed for XML loading, PNG decoding, and snapshot.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* clang-format off */

#ifndef __ASSEMBLER__
#include <stdint.h>
#endif

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 32

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_SIZE (8 * 1024 * 1024U)

/*====================
   DISPLAY SETTINGS
 *====================*/
#define LV_DPI_DEF 130

/*====================
   DRAWING
 *====================*/
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 1
#define LV_USE_DRAW_ARM2D 0
#define LV_USE_NEON 0
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE

#define LV_GRADIENT_MAX_STOPS 8

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/*====================
   FILESYSTEM
 *====================*/
#define LV_USE_FS_STDIO 1
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER 'A'
    #define LV_FS_STDIO_PATH ""
    #define LV_FS_STDIO_CACHE_SIZE 0
#endif

/*====================
   LIBS
 *====================*/
#define LV_USE_LODEPNG 1

/*====================
   OTHERS
 *====================*/
#define LV_USE_SNAPSHOT 1
#define LV_USE_XML 1

/*====================
   WIDGETS (minimal)
 *====================*/
#define LV_USE_LABEL 1
#define LV_USE_IMAGE 1
#define LV_USE_BUTTON 1
#define LV_USE_SLIDER 1

/* clang-format on */

#endif /* LV_CONF_H */
