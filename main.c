#include <pspsdk.h>
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspthreadman.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("NightDim", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define CONFIG_PATH   "ms0:/seplugins/nightdim.ini"
#define CONFIG_PATH_GO "ef0:/seplugins/nightdim.ini"

static volatile int g_running = 1;
static int g_enabled = 0;
static int g_warmth = 50;
static int g_osd_timer = 0;

static SceCtrlData g_pad, g_oldpad;

static int button_pressed(int buttons) {
    return (g_pad.Buttons & buttons) == buttons && (g_oldpad.Buttons & buttons) != buttons;
}

static void load_config(void) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) f = fopen(CONFIG_PATH_GO, "r");
    if (!f) return;
    
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int val;
        if (sscanf(line, "warmth=%d", &val) == 1) {
            if (val >= 0 && val <= 100) g_warmth = val;
        }
    }
    fclose(f);
}

static void save_config(void) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) f = fopen(CONFIG_PATH_GO, "w");
    if (!f) return;
    fprintf(f, "# NightDim config\n");
    fprintf(f, "warmth=%d\n", g_warmth);
    fclose(f);
}

static void apply_warm_filter(void *fb, int stride, int format) {
    int y, x;
    
    if (format == PSP_DISPLAY_PIXEL_FORMAT_8888) {
        u32 *pixels = (u32*)fb;
        int warmth_r = (g_warmth * 30) / 100;
        int warmth_b = (g_warmth * 40) / 100;
        
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            for (x = 0; x < SCREEN_WIDTH; x++) {
                u32 p = pixels[y * stride + x];
                int a = (p >> 24) & 0xFF;
                int r = (p >> 16) & 0xFF;
                int g = (p >> 8)  & 0xFF;
                int b = p & 0xFF;
                
                r = r + warmth_r;
                if (r > 255) r = 255;
                b = b - warmth_b;
                if (b < 0) b = 0;
                
                pixels[y * stride + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_565) {
        u16 *pixels = (u16*)fb;
        int warmth_r = (g_warmth * 8) / 100;
        int warmth_b = (g_warmth * 10) / 100;
        
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            for (x = 0; x < SCREEN_WIDTH; x++) {
                u16 p = pixels[y * stride + x];
                int r = (p >> 11) & 0x1F;
                int g = (p >> 5)  & 0x3F;
                int b = p & 0x1F;
                
                r = r + warmth_r;
                if (r > 31) r = 31;
                b = b - warmth_b;
                if (b < 0) b = 0;
                
                pixels[y * stride + x] = (r << 11) | (g << 5) | b;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_5551) {
        u16 *pixels = (u16*)fb;
        int warmth_r = (g_warmth * 8) / 100;
        int warmth_b = (g_warmth * 10) / 100;
        
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            for (x = 0; x < SCREEN_WIDTH; x++) {
                u16 p = pixels[y * stride + x];
                int a = (p >> 15) & 0x1;
                int r = (p >> 10) & 0x1F;
                int g = (p >> 5)  & 0x1F;
                int b = p & 0x1F;
                
                r = r + warmth_r;
                if (r > 31) r = 31;
                b = b - warmth_b;
                if (b < 0) b = 0;
                
                pixels[y * stride + x] = (a << 15) | (r << 10) | (g << 5) | b;
            }
        }
    }
    
    sceKernelDcacheWritebackInvalidateAll();
}

static void draw_indicator(void *fb, int stride, int format) {
    int x = 450, y = 5;
    int size = 8;
    
    if (format == PSP_DISPLAY_PIXEL_FORMAT_8888) {
        u32 *pixels = (u32*)fb;
        u32 c = 0xFFFFAA55;
        for (int dy = 0; dy < size; dy++) {
            for (int dx = 0; dx < size; dx++) {
                if ((dx + dy) % 2 == 0)
                    pixels[(y + dy) * stride + (x + dx)] = c;
            }
        }
    }
    else if (format == PSP_DISPLAY_PIXEL_FORMAT_565) {
        u16 *pixels = (u16*)fb;
        u16 c = 0xFCC0;
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
        
        if (button_pressed(PSP_CTRL_SELECT | PSP_CTRL_LTRIGGER)) {
            g_enabled = !g_enabled;
            g_osd_timer = 120;
            save_config();
        }
        
        if (g_enabled) {
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
