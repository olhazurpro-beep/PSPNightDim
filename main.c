#include <pspsdk.h>
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspthreadman.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("NightDim", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define CONFIG_PATH   "ms0:/seplugins/nightdim.ini"
#define CONFIG_PATH_GO "ef0:/seplugins/nightdim.ini"

static volatile int g_running = 1;
static int g_enabled = 0;
static int g_brightness = 35;
static int g_warmth = 50;
static int g_auto_time = 0;
static int g_auto_start = 21;
static int g_auto_end = 8;
static int g_osd_timer = 0;

static SceCtrlData g_pad, g_oldpad;

static int button_pressed(int buttons) {
    return (g_pad.Buttons & buttons) == buttons && (g_oldpad.Buttons & buttons) != buttons;
}

static int file_exists(const char *path) {
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0;
}

static void load_config(void) {
    const char *paths[] = {CONFIG_PATH, CONFIG_PATH_GO};
    FILE *f = NULL;
    for (int i = 0; i < 2; i++) {
        f = fopen(paths[i], "r");
        if (f) break;
    }
    if (!f) return;
    
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int val;
        if (sscanf(line, "brightness=%d", &val) == 1) {
            if (val >= 0 && val <= 100) g_brightness = val;
        }
        else if (sscanf(line, "warmth=%d", &val) == 1) {
            if (val >= 0 && val <= 100) g_warmth = val;
        }
        else if (sscanf(line, "auto_time=%d", &val) == 1) {
            g_auto_time = val;
        }
        else if (sscanf(line, "auto_start=%d", &val) == 1) {
            if (val >= 0 && val <= 23) g_auto_start = val;
        }
        else if (sscanf(line, "auto_end=%d", &val) == 1) {
            if (val >= 0 && val <= 23) g_auto_end = val;
        }
    }
    fclose(f);
}

static void save_config(void) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) f = fopen(CONFIG_PATH_GO, "w");
    if (!f) return;
    fprintf(f, "# NightDim config\n");
    fprintf(f, "brightness=%d\n", g_brightness);
    fprintf(f, "warmth=%d\n", g_warmth);
    fprintf(f, "auto_time=%d\n", g_auto_time);
    fprintf(f, "auto_start=%d\n", g_auto_start);
    fprintf(f, "auto_end=%d\n", g_auto_end);
    fclose(f);
}

static void check_auto_time(void) {
    if (!g_auto_time) return;
    pspTime time;
    sceRtcGetCurrentClockLocalTime(&time);
    int hour = time.hour;
    int should_be_on = 0;
    
    if (g_auto_start > g_auto_end) {
        should_be_on = (hour >= g_auto_start || hour < g_auto_end);
    } else {
        should_be_on = (hour >= g_auto_start && hour < g_auto_end);
    }
    
    if (should_be_on && !g_enabled) {
        g_enabled = 1;
        sceDisplaySetBrightness(g_brightness, 0);
    } else if (!should_be_on && g_enabled) {
        g_enabled = 0;
        sceDisplaySetBrightness(84, 0);
    }
}

static void apply_warm_filter(void *fb, int stride, int format) {
    int y, x;
    
    if (format == PSP_DISPLAY_PIXEL_FORMAT_8888) {
        uint32_t *pixels = (uint32_t*)fb;
        int warmth_r = (g_warmth * 30) / 100;
        int warmth_b = (g_warmth * 40) / 100;
        
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            for (x = 0; x < SCREEN_WIDTH; x++) {
                uint32_t p = pixels[y * stride + x];
                int a = (p >> 24) & 0xFF;
                int r = (p >> 16) & 0xFF;
                int g = (p >> 8)  & 0xFF;
                int b = p & 0xFF;
                
                r = r + warmth_r; if (r > 255) r = 255;
                b = b - warmth_b; if (b < 0) b = 0;
                
                pixels[y * stride + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_565) {
        uint16_t *pixels = (uint16_t*)fb;
        int warmth_r = (g_warmth * 8) / 100;
        int warmth_b = (g_warmth * 10) / 100;
        
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            for (x = 0; x < SCREEN_WIDTH; x++) {
                uint16_t p = pixels[y * stride + x];
                int r = (p >> 11) & 0x1F;
                int g = (p >> 5)  & 0x3F;
                int b = p & 0x1F;
                
                r = r + warmth_r; if (r > 31) r = 31;
                b = b - warmth_b; if (b < 0) b = 0;
                
                pixels[y * stride + x] = (r << 11) | (g << 5) | b;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_5551) {
        uint16_t *pixels = (uint16_t*)fb;
        int warmth_r = (g_warmth * 8) / 100;
        int warmth_b = (g_warmth * 10) / 100;
        
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            for (x = 0; x < SCREEN_WIDTH; x++) {
                uint16_t p = pixels[y * stride + x];
                int a = (p >> 15) & 0x1;
                int r = (p >> 10) & 0x1F;
                int g = (p >> 5)  & 0x1F;
                int b = p & 0x1F;
                
                r = r + warmth_r; if (r > 31) r = 31;
                b = b - warmth_b; if (b < 0) b = 0;
                
                pixels[y * stride + x] = (a << 15) | (r << 10) | (g << 5) | b;
            }
        }
    }
    
    sceKernelDcacheWritebackInvalidateAll();
}

static void draw_osd(void *fb, int stride, int format) {
    if (g_osd_timer <= 0) return;
    g_osd_timer--;
    
    int x = 200, y = 120;
    int w = 160, h = 32;
    
    if (format == PSP_DISPLAY_PIXEL_FORMAT_8888) {
        uint32_t *pixels = (uint32_t*)fb;
        uint32_t bg = 0xCC000000;
        uint32_t fg = 0xFFFFAA55;
        
        for (int dy = 0; dy < h; dy++) {
            for (int dx = 0; dx < w; dx++) {
                int px = x + dx, py = y + dy;
                if (px < SCREEN_WIDTH && py < SCREEN_HEIGHT) {
                    pixels[py * stride + px] = bg;
                }
            }
        }
        int bar_w = (g_brightness * (w - 8)) / 100;
        for (int dx = 4; dx < 4 + bar_w; dx++) {
            for (int dy = 20; dy < 28; dy++) {
                pixels[(y + dy) * stride + (x + dx)] = fg;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_565) {
        uint16_t *pixels = (uint16_t*)fb;
        uint16_t bg = 0x0000;
        uint16_t fg = 0xFCC0;
        
        for (int dy = 0; dy < h; dy++) {
            for (int dx = 0; dx < w; dx++) {
                int px = x + dx, py = y + dy;
                if (px < SCREEN_WIDTH && py < SCREEN_HEIGHT) {
                    pixels[py * stride + px] = bg;
                }
            }
        }
        int bar_w = (g_brightness * (w - 8)) / 100;
        for (int dx = 4; dx < 4 + bar_w; dx++) {
            for (int dy = 20; dy < 28; dy++) {
                pixels[(y + dy) * stride + (x + dx)] = fg;
            }
        }
    }
}

static void draw_indicator(void *fb, int stride, int format) {
    int x = 450, y = 5;
    int size = 8;
    
    if (format == PSP_DISPLAY_PIXEL_FORMAT_8888) {
        uint32_t *pixels = (uint32_t*)fb;
        uint32_t c = 0xFFFFAA55;
        for (int dy = 0; dy < size; dy++) {
            for (int dx = 0; dx < size; dx++) {
                if ((dx + dy) % 2 == 0)
                    pixels[(y + dy) * stride + (x + dx)] = c;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_565) {
        uint16_t *pixels = (uint16_t*)fb;
        uint16_t c = 0xFCC0;
        for (int dy = 0; dy < size; dy++) {
            for (int dx = 0; dx < size; dx++) {
                if ((dx + dy) % 2 == 0)
                    pixels[(y + dy) * stride + (x + dx)] = c;
            }
        }
    }
}

static int worker_thread(SceSize args, void *argp) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    
    memset(&g_pad, 0, sizeof(g_pad));
    memset(&g_oldpad, 0, sizeof(g_oldpad));
    
    load_config();
    sceKernelDelayThread(800000);
    
    while (g_running) {
        g_oldpad = g_pad;
        sceCtrlReadBufferPositive(&g_pad, 1);
        
        check_auto_time();
        
        if (button_pressed(PSP_CTRL_SELECT | PSP_CTRL_LTRIGGER)) {
            g_enabled = !g_enabled;
            g_osd_timer = 120;
            
            if (g_enabled) {
                sceDisplaySetBrightness(g_brightness, 0);
            } else {
                sceDisplaySetBrightness(84, 0);
            }
            save_config();
        }
        
        if (g_enabled) {
            if (button_pressed(PSP_CTRL_SELECT | PSP_CTRL_LEFT)) {
                if (g_brightness > 5) g_brightness -= 5;
                sceDisplaySetBrightness(g_brightness, 0);
                g_osd_timer = 90;
                save_config();
            }
            if (button_pressed(PSP_CTRL_SELECT | PSP_CTRL_RIGHT)) {
                if (g_brightness < 100) g_brightness += 5;
                sceDisplaySetBrightness(g_brightness, 0);
                g_osd_timer = 90;
                save_config();
            }
            if (button_pressed(PSP_CTRL_SELECT | PSP_CTRL_UP)) {
                if (g_warmth < 100) g_warmth += 10;
                g_osd_timer = 90;
                save_config();
            }
            if (button_pressed(PSP_CTRL_SELECT | PSP_CTRL_DOWN)) {
                if (g_warmth > 0) g_warmth -= 10;
                g_osd_timer = 90;
                save_config();
            }
        }
        
        void *fb = NULL;
        int stride = 0;
        int format = 0;
        
        int ret = sceDisplayGetFrameBuf(&fb, &stride, &format, 0);
        if (ret >= 0 && fb != NULL && g_enabled) {
            apply_warm_filter(fb, stride, format);
            draw_indicator(fb, stride, format);
        }
        
        if (g_osd_timer > 0 && fb != NULL) {
            draw_osd(fb, stride, format);
        }
        
        sceDisplayWaitVblankStart();
    }
    
    return 0;
}

int module_start(SceSize args, void *argp) {
    int thid = sceKernelCreateThread("nightdim_worker", worker_thread, 0x18, 0x4000, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
    return 0;
}

int module_stop(SceSize args, void *argp) {
    g_running = 0;
    sceKernelDelayThread(100000);
    return 0;
}
