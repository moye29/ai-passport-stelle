#ifndef LV_CONF_H
#define LV_CONF_H
/* Example Passport settings. Match the current firmware before judging parity. */
#define LV_COLOR_DEPTH 16
#ifndef PREVIEW_LV_MEM_KB
#define PREVIEW_LV_MEM_KB 24U  /* must match CONFIG_LV_MEM_SIZE_KILOBYTES */
#endif
#define LV_FONT_UNSCII_8 1
#define LV_FONT_UNSCII_16 1
#define LV_MEM_SIZE (PREVIEW_LV_MEM_KB * 1024U)
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_SYSMON 0
#define LV_USE_QRCODE 1  /* matches CONFIG_LV_USE_QRCODE (Wi-Fi setup screen) */
/* Fail fast instead of spinning forever on an assertion (e.g. pool exhausted). */
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();
#endif
