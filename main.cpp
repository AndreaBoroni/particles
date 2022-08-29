#include <windows.h>
#include <intrin.h>
#include <string>
#include <stdlib.h>
#include <xmmintrin.h>

using namespace std;

/* Todo list:
    - Optimize
    - Save with the correct number
UI:
    - Better rendering in the window
*/

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "gif.h"

#define UpDown_(var) ((void *) &(var))
#define start_sliders() int slider_count = 0;
#define new_slider()    ++slider_count

#define get_time(t)  QueryPerformanceCounter((t));
#define time_elapsed(t0, t1, CPM) ((t1).QuadPart - (t0).QuadPart) / (CPM);

#define MAX(a, b) (((a) > (b)) ? (a) : (b));
#define MIN(a, b) (((a) < (b)) ? (a) : (b));
#define clamp(a, b, c) (((a) > (c)) ? (c) : (((a) < (b)) ? (b) : (a)))

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;
typedef LARGE_INTEGER      ms;

typedef char      int8;
typedef short     int16;
typedef int       int32;
typedef long long int64;

#define Bytes_Per_Pixel 4
struct bitmap
{
    uint8 *Memory = NULL;
    int32 Width   = 0;
    int32 Height  = 0;

    uint8 *Original  = NULL;
    BITMAPINFO *Info = NULL;
};

const int file_name_size = 400;

struct RECT_f {
    float left, top, right, bottom;
};

RECT rect_from_rectf(RECT_f rect) {
    RECT result;
    result.left   = rect.left;
    result.top    = rect.top;
    result.right  = rect.right;
    result.bottom = rect.bottom;

    return result;
}

struct v2 {
    int x, y;
};

bitmap     Main_Buffer = {0};
BITMAPINFO Main_Info   = {0};

HWND Window = {0};

bool left_button_down  = false;
bool right_button_down = false;

int mousewheel_counter = 0;
v2  mouse_position     = {0, 0};

bool handled_press_left  = false;
bool handled_press_right = false;
bool changed_size = false;

#define FIRST_CHAR_SAVED 32
#define LAST_CHAR_SAVED  126
#define CHAR_SAVED_RANGE LAST_CHAR_SAVED - FIRST_CHAR_SAVED + 1

#define N_SIZES 3
struct Font_Data {
    float scale[N_SIZES];
    
    int ascent;
    int descent;
    
    int advance;
    int line_height;
    int line_gap;
    
    stbtt_fontinfo info;

    stbtt_pack_context spc[N_SIZES];
    stbtt_packedchar   chardata[N_SIZES][CHAR_SAVED_RANGE];
};

enum font_types {
    Small_Font  = 0,
    Medium_Font = 1,
    Big_Font    = 2,
};

Font_Data Font;

struct Color {
    int R, G, B, A;
};

struct Color_hsv {
    float h, s, v;
    uint8 alpha;
};
inline max_c(uint8 a, uint8 b, uint8 c) {
    uint8 temp = MAX(a, b);
    return MAX(temp, c);
}
inline min_c(uint8 a, uint8 b, uint8 c) {
    uint8 temp = MIN(a, b);
    return MIN(temp, c);
}
Color_hsv rgb_to_hsv(Color c) {
    uint8 min = min_c(c.R, c.G, c.B);
    uint8 max = max_c(c.R, c.G, c.B);
    uint8 delta = max - min;

    Color_hsv result;
    result.alpha = c.A;
    result.v     = (float) max / 255.0;

    if (max > 0) result.s = (float) delta / (float) max;
    else {
        result.s = 0;
        result.h = 0;
        return result;
    }

    if      (c.R == max) result.h = (float) (c.G - c.B) / (float) delta;
    else if (c.G == max) result.h = (float) (c.B - c.R) / (float) delta + 2;
    else                 result.h = (float) (c.R - c.G) / (float) delta + 4;

    result.h *= 60;
    if (result.h < 0) result.h += 360;

    return result;
}

Color hsv_to_rgb(Color_hsv c) {

    uint8 v = c.v * 255;
    if (c.s == 0) return {v, v, v, c.alpha};

    c.h /= 60; 
    int sector = floor(c.h);
    float diff = c.h - sector;
    uint8 p = v * (1.0 - c.s);
    uint8 q = v * (1.0 - c.s * diff);
    uint8 t = v * (1.0 - c.s * (1.0 - diff));
    
    switch (sector) {
        case 0: return {v, t, p, c.alpha};
        case 1: return {q, v, p, c.alpha};
        case 2: return {p, v, t, c.alpha};
        case 3: return {p, q, v, c.alpha};
        case 4: return {t, p, v, c.alpha};
        case 5: return {v, p, q, c.alpha};
    }
    return {0, 0, 0, c.alpha};
}

Color shift_hue(Color c, int shift) {
    Color_hsv hsv = rgb_to_hsv(c);
    
    hsv.h += shift;
    while(hsv.h >= 360) hsv.h -= 360;
    while(hsv.h <  0)   hsv.h += 360;

    return hsv_to_rgb(hsv);
}

const Color WHITE      = {255, 255, 255, 255};
const Color DARK_WHITE = {180, 180, 180, 255};
const Color LIGHT_GRAY = { 95,  95,  95, 255};
const Color GRAY       = { 50,  50,  50, 255};
const Color BLACK      = {  0,   0,   0, 255};
const Color GREEN      = { 70, 200,  80, 255};
const Color DARK_GREEN = { 50, 125,  60, 255};
const Color BLUE       = { 70, 200, 255, 255};
const Color DARK_BLUE  = { 20, 120, 170, 255};
const Color RED        = {255,   0,   0, 255};
const Color ERROR_RED  = {201,  36,  24, 255};
const Color WARNING    = {247, 194,  32, 255};

struct Color_Palette {
    Color button_color, highlight_button_color;
    Color value_color,  highlight_value_color;
    Color text_color,   highlight_text_color;
    Color background_color;
};

//                               btn         h_btn  val         h_val  txt        h_txt  bg
Color_Palette default_palette = {DARK_WHITE, WHITE, BLUE,       BLUE,  DARK_BLUE, BLUE,  BLACK};
Color_Palette slider_palette  = {DARK_WHITE, WHITE, BLACK,      BLACK, DARK_BLUE, BLUE,  BLACK};
Color_Palette header_palette  = {LIGHT_GRAY, WHITE, BLACK,      BLACK, WHITE,     BLACK, GRAY};
Color_Palette save_palette    = {DARK_WHITE, WHITE, BLACK,      BLACK, BLACK,     BLACK, BLUE};
Color_Palette no_save_palette = {DARK_WHITE, WHITE, BLACK,      BLACK, DARK_BLUE, BLUE,  BLACK};
Color_Palette slider_palette2 = {DARK_WHITE, WHITE, DARK_WHITE, WHITE, DARK_BLUE, BLUE,  BLACK};

const int Packed_Font_W = 500;
const int Packed_Font_H = 500;

RECT get_rect(int x, int y, int width, int height) {
    return {x, y, x + width, y + height};
}

void init_font(int sizes[]) {
    char ttf_buffer[1<<20];
    int temp;

    fread(ttf_buffer, 1, 1<<20, fopen("Font/consola.ttf", "rb"));

    stbtt_InitFont(&Font.info, (const uint8 *) ttf_buffer, stbtt_GetFontOffsetForIndex((const uint8 *) ttf_buffer, 0));
    stbtt_GetFontVMetrics(&Font.info, &Font.ascent, &Font.descent, &Font.line_gap);
    stbtt_GetCodepointHMetrics(&Font.info, 'A', &Font.advance, &temp);
    Font.line_height = (Font.ascent - Font.descent + Font.line_gap);

    uint8 temp_bitmap[N_SIZES][Packed_Font_W][Packed_Font_H];

    for(int i = 0; i < N_SIZES; i++) {
        Font.scale[i] = (float) sizes[i] / (float) (Font.ascent - Font.descent);
        stbtt_PackBegin(&Font.spc[i], &temp_bitmap[i][0][0], Packed_Font_W, Packed_Font_H, 0, 1, NULL);
        stbtt_PackFontRange(&Font.spc[i], (const uint8 *) ttf_buffer, 0, sizes[i], FIRST_CHAR_SAVED, CHAR_SAVED_RANGE, Font.chardata[i]);    
        stbtt_PackEnd(&Font.spc[i]);
    }
}

void render_text(char *text, int length, int font_type, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    float xpos = (dest_rect.left + dest_rect.right) / 2 - (length * Font.advance * Font.scale[font_type]) / 2;
    float ypos = (dest_rect.top + dest_rect.bottom) / 2 + Font.ascent * Font.scale[font_type] / 2;
    stbtt_aligned_quad quad;

    int x, y;
    
    for (uint8 ch = 0; ch < length; ch++) {

        stbtt_GetPackedQuad(&Font.chardata[font_type][0], 1, 1, text[ch] - FIRST_CHAR_SAVED, &xpos, &ypos, &quad, true);
        
        uint32 *Dest   = (uint32 *) Main_Buffer.Memory + (uint32) (Main_Buffer.Width * quad.y0 + quad.x0);
        uint8  *Source = Font.spc[font_type].pixels + (uint32) (Packed_Font_W * quad.t0 + quad.s0);

        int max_x = MIN(xpos + quad.x1 - quad.x0, Main_Buffer.Width);
        int max_y = MIN(ypos + quad.y1 - quad.y0, Main_Buffer.Height);

        int skipped_horizontal_pixels = (xpos + quad.x1 - quad.x0) - max_x;
        for (y = ypos; y < max_y; y++) {
            for (x = xpos; x < max_x; x++) {
                if (xpos > 0 && ypos > 0) {
                    float SA = *Source / 255.0;
                    uint8 SR = c.R * SA;
                    uint8 SG = c.G * SA;
                    uint8 SB = c.B * SA;

                    float DA = ((*Dest >> 24) & 0xff) / 255.0;
                    uint8 DR =  (*Dest >> 16) & 0xff;
                    uint8 DG =  (*Dest >>  8) & 0xff;
                    uint8 DB =  (*Dest >>  0) & 0xff;

                    uint8 A = 255 * (SA + DA - SA*DA);
                    uint8 R = DR * (1 - SA) + SR;
                    uint8 G = DG * (1 - SA) + SG;
                    uint8 B = DB * (1 - SA) + SB;

                    *Dest = (A << 24) | (R << 16) | (G << 8) | B;
                }
                Source++;
                Dest++;
            }
            Source += (uint32) (Packed_Font_W     - (quad.s1 - quad.s0) + skipped_horizontal_pixels);
            Dest   += (uint32) (Main_Buffer.Width - (max_x - xpos));
        }

        xpos = ceil(xpos);
    }
}

void render_text(int number, int font_type, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    char *text = (char *)to_string(number).c_str();
    
    int length;
    for (length = 0; text[length] != '\0'; length++) {}
    
    render_text(text, length, font_type, dest_rect, c);
}

void render_text(float number, int digits, int font_type, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    char *text = (char *)to_string(number).c_str();
    
    int length;
    for (length = 0; text[length] != '\0'; length++) {}
    length = length < digits + 2 ? length : digits + 2;

    render_text(text, length, font_type, dest_rect, c);
}

void initialize_main_buffer() {
    Main_Buffer.Info = &Main_Info;
    Main_Buffer.Info->bmiHeader.biSize        = sizeof(Main_Buffer.Info->bmiHeader);
    Main_Buffer.Info->bmiHeader.biPlanes      = 1;
    Main_Buffer.Info->bmiHeader.biBitCount    = 8 * Bytes_Per_Pixel;
    Main_Buffer.Info->bmiHeader.biCompression = BI_RGB;
    Main_Buffer.Memory = NULL;
}

void resize_main_buffer(int new_width, int new_height) {

    if (Main_Buffer.Memory) free(Main_Buffer.Memory);

    Main_Buffer.Info->bmiHeader.biWidth  =  new_width;
    Main_Buffer.Info->bmiHeader.biHeight = -new_height;
    Main_Buffer.Memory = (uint8 *) malloc((new_width * new_height) * Bytes_Per_Pixel);
    Main_Buffer.Width  = new_width;
    Main_Buffer.Height = new_height;
}

void blit_main_buffer_to_window() {
    HDC DeviceContext = GetDC(Window);

    StretchDIBits(DeviceContext,
                  0, 0, Main_Buffer.Width, Main_Buffer.Height, // destination
                  0, 0, Main_Buffer.Width, Main_Buffer.Height, // source
                  Main_Buffer.Memory, Main_Buffer.Info, DIB_RGB_COLORS, SRCCOPY);
}

v2 screen_to_window_position(POINT pos) {
    v2 result;

    RECT rect;
    GetClientRect( Window, (LPRECT) &rect);
    ClientToScreen(Window, (LPPOINT)&rect.left);
    ClientToScreen(Window, (LPPOINT)&rect.right);
    result.x = pos.x - rect.left;
    result.y = pos.y - rect.top;

    return result;
}

LRESULT CALLBACK main_window_callback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_SIZE: {
            changed_size = true;

            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int width  = ClientRect.right  - ClientRect.left;
            int height = ClientRect.bottom - ClientRect.top;

            resize_main_buffer(width, height);
        } break;
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;
        case WM_CLOSE: {
            PostQuitMessage(0);
        } break;
        case WM_LBUTTONDOWN:
            left_button_down   = true;
            handled_press_left = false;
            break;
        case WM_LBUTTONUP:
            left_button_down   = false;
            handled_press_left = true;
            break;
        case WM_RBUTTONDOWN:
            right_button_down   = true;
            handled_press_right = false;
            break;
        case WM_RBUTTONUP:
            right_button_down   = false;
            handled_press_right = true;
            break;
        case WM_MOUSEWHEEL: {
            auto key_state = GET_KEYSTATE_WPARAM(WParam);
            if (key_state) break;

            auto delta = GET_WHEEL_DELTA_WPARAM(WParam);
            mousewheel_counter += delta;
        } break;
        default: {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

#define INITIAL_WIDTH  1120
#define INITIAL_HEIGHT 750

void start_main_window() {
    WNDCLASS WindowClass = {};
    WindowClass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc   = main_window_callback;
    WindowClass.hInstance     = GetModuleHandle(0);
    WindowClass.lpszClassName = "GeneralWindowClass";

    RegisterClass(&WindowClass);
    Window = CreateWindowEx(
        0,
        WindowClass.lpszClassName,
        "Game of Life",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE | WS_THICKFRAME,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        INITIAL_WIDTH,
        INITIAL_HEIGHT,
        0, 0, GetModuleHandle(0), 0);
}

int handle_window_messages() {
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
        if (Message.message == WM_QUIT) return -1;
    }
    return 0;
}

void render_filled_rectangle(RECT rectangle, Color color)
{
    uint32 c = (color.A << 24) | (color.R << 16) | (color.G << 8) | color.B;
    
    int starting_x = MAX(rectangle.left,   0);
    int starting_y = MAX(rectangle.top,    0);
    int ending_x   = MIN(rectangle.right,  Main_Buffer.Width);
    int ending_y   = MIN(rectangle.bottom, Main_Buffer.Height);
    
    if (starting_x > Main_Buffer.Width)  return;
    if (starting_y > Main_Buffer.Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int Pitch = Main_Buffer.Width * Bytes_Per_Pixel;
    uint8 *Row = Main_Buffer.Memory;
    Row += starting_x*Bytes_Per_Pixel + starting_y*Pitch;

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;   
        for (int X = starting_x; X < ending_x; X++) {
            *Pixel = c;
            Pixel++;
        }
        Row += Pitch;
    }
}

void render_rectangle(RECT rectangle, Color color, uint8 thickness)
{
    if (thickness * 2 >= rectangle.right - rectangle.left || thickness * 2 >= rectangle.bottom - rectangle.top) {
        render_filled_rectangle(rectangle, color);
        return;
    }

    RECT top_rect    = {rectangle.left, rectangle.top, rectangle.right, rectangle.top + thickness};
    RECT bottom_rect = {rectangle.left, rectangle.bottom - thickness, rectangle.right, rectangle.bottom};
    RECT left_rect   = {rectangle.left, rectangle.top + thickness, rectangle.left + thickness, rectangle.bottom - thickness};
    RECT right_rect  = {rectangle.right - thickness, rectangle.top + thickness, rectangle.right, rectangle.bottom - thickness};

    render_filled_rectangle(top_rect, color);
    render_filled_rectangle(bottom_rect, color);
    render_filled_rectangle(left_rect, color);
    render_filled_rectangle(right_rect, color);
}

void blit_bitmap_to_bitmap(bitmap *Dest, bitmap *Source, int x, int y, int width, int height) {

    if (width  < 0) width  = Source->Width;
    if (height < 0) height = Source->Height;

    int starting_x = MAX(x, 0);
    int starting_y = MAX(y, 0);
    int ending_x   = MIN(x + width,  Dest->Width);
    int ending_y   = MIN(y + height, Dest->Height);
    
    if (starting_x > Dest->Width)  return;
    if (starting_y > Dest->Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int x_bitmap, y_bitmap;

    int Dest_Pitch = Dest->Width * Bytes_Per_Pixel;
    uint8 *Row     = (uint8 *)  Dest->Memory;
    uint32 *Texels = (uint32 *) Source->Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Dest_Pitch;

    float width_scale  = (float) Source->Width  / (float) width;
    float height_scale = (float) Source->Height / (float) height;

    for (int Y = starting_y; Y < ending_y; Y++) {
        uint32 *Pixel = (uint32 *)Row;
        y_bitmap = (Y - y) * height_scale;
        for (int X = starting_x; X < ending_x; X++) {
            x_bitmap = (X - x) * width_scale;
            uint32 source_pixel = Texels[x_bitmap + y_bitmap * Source->Width];

            float SA = ((source_pixel >> 24) & 0xff) / 255.0;
            uint8 SR =  (source_pixel >> 16) & 0xff;
            uint8 SG =  (source_pixel >>  8) & 0xff;
            uint8 SB =  (source_pixel >>  0) & 0xff;

            float DA = ((*Pixel >> 24) & 0xff) / 255.0;
            uint8 DR =  (*Pixel >> 16) & 0xff;
            uint8 DG =  (*Pixel >>  8) & 0xff;
            uint8 DB =  (*Pixel >>  0) & 0xff;

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (R << 16) | (G << 8) | B;
            Pixel++;
        }
        Row += Dest_Pitch;
    }
}

int get_w(RECT rect)   { return rect.right  - rect.left; }
int get_h(RECT rect)   { return rect.bottom - rect.top; }
int get_w(RECT_f rect) { return rect.right  - rect.left; }
int get_h(RECT_f rect) { return rect.bottom - rect.top; }

void render_bitmap_to_screen(bitmap *Source, RECT dest_rect, RECT source_rect) {
    int starting_x = MAX(dest_rect.left, 0);
    int starting_y = MAX(dest_rect.top,  0);
    int ending_x   = MIN(dest_rect.right,  Main_Buffer.Width);
    int ending_y   = MIN(dest_rect.bottom, Main_Buffer.Height);
    
    if (starting_x > Main_Buffer.Width)  return;
    if (starting_y > Main_Buffer.Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int x_bitmap, y_bitmap;

    int Dest_Pitch = Main_Buffer.Width * Bytes_Per_Pixel;
    uint8 *Row     = (uint8 *)  Main_Buffer.Memory;
    uint32 *Texels = (uint32 *) Source->Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Dest_Pitch;

    float width_scale  = (float) get_w(source_rect) / (float) get_w(dest_rect);
    float height_scale = (float) get_h(source_rect) / (float) get_h(dest_rect);

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;
        
        y_bitmap = (Y - dest_rect.top) * height_scale;
        if (y_bitmap + source_rect.top  >= Source->Height || y_bitmap + source_rect.top  < 0) {
            Row += Dest_Pitch;
            continue;
        }

        for (int X = starting_x; X < ending_x; X++) {

            x_bitmap = (X - dest_rect.left) * width_scale;
            if (x_bitmap + source_rect.left < 0 || x_bitmap + source_rect.left >= Source->Width) {
                Pixel++;
                continue;
            }

            uint32 texel_position = (x_bitmap + source_rect.left) + (y_bitmap + source_rect.top) * Source->Width;
            uint32 source_pixel = Texels[texel_position];

            float SA = ((source_pixel >> 24) & 0xff) / 255.0;
            uint8 SR =  (source_pixel >> 16) & 0xff;
            uint8 SG =  (source_pixel >>  8) & 0xff;
            uint8 SB =  (source_pixel >>  0) & 0xff;

            float DA = ((*Pixel >> 24) & 0xff) / 255.0;
            uint8 DR =  (*Pixel >> 16) & 0xff;
            uint8 DG =  (*Pixel >>  8) & 0xff;
            uint8 DB =  (*Pixel >>  0) & 0xff;

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (R << 16) | (G << 8) | B;
            Pixel++;
        }
        Row += Dest_Pitch;
    }

}

#define SQRT_2 1.41421356237
#define SQRT_3 1.73205080757

RECT compute_rendering_position(RECT dest_rect, int dest_side, int source_width, int source_height) {
    RECT result;

    result.left = dest_rect.left + dest_side;
    result.top  = dest_rect.top  + dest_side;
    int rect_width  = dest_rect.right  - dest_rect.left - dest_side * 2;
    int rect_height = dest_rect.bottom - dest_rect.top  - dest_side * 2;

    bool width_or_height = (float) rect_width / (float) source_width > (float) rect_height / (float) source_height;
    int rendered_width  = width_or_height ? rect_height * source_width / source_height : rect_width;
    int rendered_height = width_or_height ? rect_height : rect_width * source_height / source_width;

    result.right  = result.left + rendered_width;
    result.bottom = result.top  + rendered_height;

    return result;
}

inline bool v2_inside_rect(v2 v, RECT rect) {
    if (v.x < rect.left)   return false;
    if (v.x > rect.right)  return false;
    if (v.y < rect.top)    return false;
    if (v.y > rect.bottom) return false;
    return true;
}

// This needs some dll
bool open_file_externally(char *file_name, int size_file_name) {
    OPENFILENAME dialog_arguments;

    memset(&dialog_arguments, 0, sizeof(dialog_arguments));
    dialog_arguments.lStructSize = sizeof(dialog_arguments);
    dialog_arguments.hwndOwner = Window;
    dialog_arguments.lpstrFile = file_name;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    dialog_arguments.lpstrFile[0] = '\0';
    dialog_arguments.nMaxFile = size_file_name;
    dialog_arguments.lpstrFilter = "All\0*.*\0";
    dialog_arguments.nFilterIndex = 1;
    dialog_arguments.lpstrFileTitle = NULL;
    dialog_arguments.nMaxFileTitle = 0;
    dialog_arguments.lpstrInitialDir = NULL;
    dialog_arguments.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileName(&dialog_arguments);
}

bool strings_match(char *s1, char *s2, int length = -1) {
    if (length < 0) length = strlen(s1);

    for (int i = 0; i < length; i++)
        if (s1[i] != s2[i]) return false;

    return true;
}

int get_last_index(char *file_name, char item) {
    int result = 0;
    for (int i = 0; true; i++) {
        if (file_name[i] == '\0') return result;
        if (file_name[i] == item)  result = i;
    }
    return 0;
}

struct Slider_Handler {
    bool pressing_a_slider       = false;
    int  which_slider_is_pressed = -1;
    bool high_slider_pressed     = false; // for double sliders
};

Slider_Handler sliders;

struct Panel {
    int left_x = 0;

    int base_width  = 0;
    int base_height = 0;

    int row_height   = 0;
    int column_width = 0;

    int at_x = 0;
    int at_y = 0;

    int current_row    = 0;
    int current_column = 0;
    int columns_this_row = 0;

    bool adaptive_row = false;

    int font_size = Small_Font;

    void row(int columns = 1, float height_factor = 1, bool adaptive = false);
    void empty_row(float height_factor = 1);
    void indent(float indent_percentage = 0.1);
    RECT get_current_rect();

    bool push_toggler(char *name, Color_Palette palette, bool *toggled, Color *override_color = NULL);
    int  push_updown_counter(char *name, Color_Palette palette, void *value, bool is_float = false);
    int  push_updown_counter(Color c, Color_Palette palette, void *value, bool *color_pressed, bool highlight_border = false);
    bool push_slider(char *text, Color_Palette palette, int *value, int min_v, int max_v, int slider_order);
    bool push_slider_with_colors(Color_Palette palette, int *value, int min_v, int max_v, Color c0, Color c1, int slider_order);
    bool push_double_slider(char *text, Color_Palette palette, int *bottom_value, int *top_value, int min_v, int max_v, int slider_order, int extra_value);
    int  push_button(char *text, Color_Palette palette, int thickness = 2);
    bool push_header(char *text, Color_Palette palette, int header, int *current_header);
    int  push_selector(char *text, Color_Palette palette);
    void add_text(char *title, Color c, int override_font_size = -1);
};

enum Push_Result {
    Button_not_Pressed_nor_Hovered,
    Button_Hovered,
    Button_Left_Clicked,
    Button_Right_Clicked,
};

Panel make_panel(int x, int y, int height, int width, int font_size) {
    Panel result;

    result.left_x = x;
    result.at_x   = x;
    result.at_y   = y;

    result.base_width  = width;
    result.base_height = height;

    result.row_height   = 0;
    result.column_width = 0;

    result.font_size = font_size;
    return result;
}

struct Settings {
    // simulation
    int gravity_divider = 20;
    int entropy_scale = 50;
    int max_distance = 80;
    int *gravities = NULL;

    // particles
    int particle_types = 4;
    Color *particle_colors = NULL;
    int   *particle_counts = NULL;
    int total_particles;

    // show
    bool gravities_visible = true;

};
Settings settings;

RECT_f reset_zoom_rectangle(bitmap image, RECT render_rect) {

    float scale_w = (float) image.Width  / (float) get_w(render_rect);
    float scale_h = (float) image.Height / (float) get_h(render_rect);

    float w = scale_w > scale_h ? image.Width  : get_w(render_rect) * scale_h;
    float h = scale_w < scale_h ? image.Height : get_h(render_rect) * scale_w;

    float delta_x = (image.Width  - w) / 2;
    float delta_y = (image.Height - h) / 2;

    RECT_f result = {delta_x, delta_y, w + delta_x, h + delta_y};

    return result;
}

void update_zoom(RECT_f *zoom_rect, RECT render_rect, float delta, v2 zoom_center) {
    float w = get_w(*zoom_rect);
    float h = get_h(*zoom_rect);

    float click_x = zoom_center.x - render_rect.left;
    float click_y = zoom_center.y - render_rect.top;

    float percentage_x = click_x / (float) get_w(render_rect);
    float percentage_y = click_y / (float) get_h(render_rect);

    float hover_image_x = percentage_x * w + zoom_rect->left;
    float hover_image_y = percentage_y * h + zoom_rect->top;

    w *= (1 + delta);
    h = w * (float) get_h(render_rect) / (float) get_w(render_rect);

    zoom_rect->left   = hover_image_x - w * percentage_x;
    zoom_rect->right  = zoom_rect->left + w;
    zoom_rect->top    = hover_image_y - h * percentage_y;
    zoom_rect->bottom = zoom_rect->top + h;
}

int get_CPM() {
    LARGE_INTEGER counter_per_second;
    QueryPerformanceFrequency(&counter_per_second);
    return counter_per_second.QuadPart / 1000;
}

bool save_file_into_memory(char *file_name, char *data, int file_size) {
    
    HANDLE file_handle;
    file_handle = CreateFile(file_name, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if(file_handle == INVALID_HANDLE_VALUE) {
        printf("ERROR: unable to create file handle\n");
        return false;
    }

    DWORD number_of_bytes_written, error;
    bool success = WriteFile(file_handle, data, file_size, &number_of_bytes_written, NULL);
    bool end_of_file_success = SetEndOfFile(file_handle);
    CloseHandle(file_handle);

    if(number_of_bytes_written != file_size) printf("Not everything was written!!\n");
    
    if (!success || !end_of_file_success) {
        error = GetLastError();
        printf("ERROR while reading the file %s. Error code: %d\n", file_name, error);
        return false;
    }
    return true;
}

char *load_file_memory(char *file_name, unsigned int *size) {
    char *buffer = NULL;
    DWORD number_of_bytes_read, high_file_size, error;
    HANDLE file_handle;

    file_handle = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file_handle == INVALID_HANDLE_VALUE) {
        printf("ERROR: unable to create file handle\n");
        return NULL;
    }

    *size = GetFileSize(file_handle, &high_file_size);

    buffer = (char *) malloc(*size);
    if (buffer == NULL) {
        printf("ERROR while allocating %d bytes ofmemory for loadign file: %s\n", *size, file_name);
        return 0;
    }
    bool result = ReadFile(file_handle, buffer, *size, &number_of_bytes_read, NULL);
    CloseHandle(file_handle);

    if (!result) {
        error = GetLastError();
        printf("ERROR while reading the file %s. Error code: %d\n", file_name, error);
    }
    return buffer;
}

struct Particle {
    float x, y;
    float vx, vy;
    int type;
};

Particle *p = 0;

void randomize_particles(Particle *p, float min_x, float max_x, float min_y, float max_y) {
    int running_total = settings.particle_counts[0];
    int running_index = 0;

    for (int i = 0; i < settings.total_particles; i++) {
        p[i].x = ((float) rand() / (float) RAND_MAX) * (max_x - min_x) + min_x;
        p[i].y = ((float) rand() / (float) RAND_MAX) * (max_y - min_y) + min_y;

        p[i].vx = 0.0;
        p[i].vy = 0.0;

        if (running_total == i) {
            running_index++;
            running_total += settings.particle_counts[running_index];
        }

        p[i].type = running_index;
    }
}

int min_x = 0;
int max_x = 1000;
int min_y = 0;
int max_y = 800;

void update_particles() {
    for (int i = 0; i < settings.total_particles; i++) {
        for (int j = 0; j < settings.total_particles; j++) {
            float G = (float) settings.gravities[p[j].type + settings.particle_types * p[i].type] / (float) settings.gravity_divider;
            float dx = p[j].x - p[i].x;
            float dy = p[j].y - p[i].y;
            float d = sqrt(dx*dx + dy*dy);
            if (d == 0 || d > settings.max_distance) continue;

            float F = G / d;
            float entropy = (float) settings.entropy_scale / 100.0f;
            p[i].vx = (p[i].vx + F * dx) * entropy;
            p[i].vy = (p[i].vy + F * dy) * entropy;

            p[i].x += p[i].vx;
            p[i].y += p[i].vy;

            if (p[i].x <= min_x) p[i].x = min_x + 1; p[i].vx *= -1;
            if (p[i].x >= max_x) p[i].x = max_x - 1; p[i].vx *= -1;

            if (p[i].y <= min_y) p[i].y = min_y + 1; p[i].vy *= -1;
            if (p[i].y >= max_y) p[i].y = max_y - 1; p[i].vy *= -1;

        }
    }
}

void randomize_weights() {
    for (int i = 0; i < settings.particle_types * settings.particle_types; i++) {
        settings.gravities[i] = rand() % 101 - 50;
    }
}

void init_particles() {
    settings.particle_types = 3;

    settings.particle_colors = (Color *) malloc(settings.particle_types * sizeof(Color));
    settings.particle_counts = (int *)   malloc(settings.particle_types * sizeof(int));
    settings.gravities       = (int *)   malloc(settings.particle_types * settings.particle_types * sizeof(int));

    settings.particle_colors[0] = RED;
    settings.particle_counts[0] = 200;
    settings.particle_colors[1] = BLUE;
    settings.particle_counts[1] = 200;
    settings.particle_colors[2] = GREEN;
    settings.particle_counts[2] = 200;
    
    settings.total_particles = 600;

    if (p) free(p);
    p = (Particle *) malloc(settings.total_particles * sizeof(Particle));
}

void resize_particle_count() {
    settings.total_particles = 0;
    for (int i = 0; i < settings.particle_types; i++) settings.total_particles += settings.particle_counts[i];

    if (p) free(p);
    p = (Particle *) malloc(settings.total_particles * sizeof(Particle));

    randomize_particles(p, min_x, max_x, min_y, max_y);
}

void add_new_particle_type() {
    settings.particle_types++;
    settings.particle_colors = (Color *) realloc(settings.particle_colors, settings.particle_types * sizeof(Color));
    settings.particle_counts = (int *)   realloc(settings.particle_counts, settings.particle_types * sizeof(int));
    settings.gravities       = (int *)   realloc(settings.gravities, settings.particle_types * settings.particle_types * sizeof(int));
    memset(settings.gravities, 0, sizeof(int) * settings.particle_types * settings.particle_types);

    Color c;
    c.R = rand() % 256;
    c.B = rand() % 256;
    c.G = rand() % 256;

    settings.particle_colors[settings.particle_types-1] = c;
    settings.particle_counts[settings.particle_types-1] = 200;

    resize_particle_count();
}

void remove_particle_type(int index) {

    for (int i = index; i < settings.particle_types-1; i++) {
        settings.particle_colors[i] = settings.particle_colors[i+1];
        settings.particle_counts[i] = settings.particle_counts[i+1];
    }
    settings.particle_types--;
    resize_particle_count();
}

struct Color_Picker {
    int x, y, side;
    Color bg_color;
    Color border_color;
    bool active = false;

    int color_index;
    Color current_color;
    Color_hsv current_color_hsv;
};

float sign(int p1x, int p1y, int p2x, int p2y, int p3x, int p3y) {
    return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
}

bool is_point_inside_triangle(int x,  int y,  int ax, int ay,
                              int bx, int by, int cx, int cy) {
    float d1, d2, d3;
    bool has_neg, has_pos;

    d1 = sign(x, y, ax, ay, bx, by);
    d2 = sign(x, y, bx, by, cx, cy);
    d3 = sign(x, y, cx, cy, ax, ay);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

bool is_point_inside_angle(int x, int y, int cx, int cy,
                           float ax, float ay, float bx, float by,
                           bool more_than_half) {
    bool b_0 = ((cx - x)*(ay - y) - (cy - y)*(ax - x)) >  0;
    bool b_1 = ((cx - x)*(by - y) - (cy - y)*(bx - x)) <= 0;

    if (more_than_half) return b_0 || b_1;
    else                return b_0 && b_1;
}

void render_circular_angle(Color color, float center_x, float center_y, float inner_radius, float outer_radius, float theta0, float theta1) {
    // theta in degrees (0 - 360): 0 is the right-most point of the circle
    double cos_theta0 = cos(theta0 / 360 * 4 * acos(0.0));
    double cos_theta1 = cos(theta1 / 360 * 4 * acos(0.0));
    double sin_theta0 = sin(theta0 / 360 * 4 * acos(0.0));
    double sin_theta1 = sin(theta1 / 360 * 4 * acos(0.0));

    float ax, ay, bx, by;
    ax = center_x - cos_theta0 * outer_radius;
    ay = center_y + sin_theta0 * outer_radius;
    bx = center_x - cos_theta1 * outer_radius;
    by = center_y + sin_theta1 * outer_radius;

    bool more_than_half = (abs(theta0 - theta1) > 180);

    uint32 c = (color.A << 24) | (color.R << 16) | (color.G << 8) | color.B;

    int starting_x = max((int) (center_x - outer_radius - 1), 0);
    int starting_y = max((int) (center_y - outer_radius - 1), 0);
    int ending_x   = min((int) (center_x + outer_radius + 1), Main_Buffer.Width  - 1);
    int ending_y   = min((int) (center_y + outer_radius + 1), Main_Buffer.Height - 1);

    if (starting_x > Main_Buffer.Width)  return;
    if (starting_y > Main_Buffer.Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int Pitch = Main_Buffer.Width*Bytes_Per_Pixel;
    uint8 *Row = (uint8 *) Main_Buffer.Memory;
    Row += starting_x*Bytes_Per_Pixel + starting_y*Pitch;

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;
        for (int X = starting_x; X < ending_x; X++) {
            int sq_dist = (center_x - X - 0.5) * (center_x - X - 0.5) + (center_y - Y - 0.5) * (center_y - Y - 0.5);
            if (sq_dist <= outer_radius * outer_radius && sq_dist >= inner_radius * inner_radius) {                
                if (is_point_inside_angle(X, Y, center_x, center_y, ax, ay, bx, by, more_than_half)) {
                    *Pixel = c;
                }
            }
            Pixel++;
        }
        Row += Pitch;
    }
}

Color click_color_picker(Color_Picker cp) {
    Color result = cp.current_color;

    // Render background
    render_filled_rectangle(get_rect(cp.x, cp.y, cp.side, cp.side), cp.bg_color);
    render_rectangle(get_rect(cp.x, cp.y, cp.side, cp.side), cp.border_color, 4);

    float inner_radius = (cp.side / 2) * 0.8;
    float outer_radius = (cp.side / 2) * 0.95;

    // Render outer circle
    {
        int Pitch = Main_Buffer.Width * Bytes_Per_Pixel;
        uint8 *Row = Main_Buffer.Memory;
        Row += cp.x*Bytes_Per_Pixel + cp.y*Pitch;
        
        for (int x = 0; x < cp.side; x++) {
            uint32 *Pixel = (uint32 *)Row;   
            for (int y = 0; y < cp.side; y++) {
                float dx = x - cp.side/2;
                float dy = y - cp.side/2;
                float r = sqrt(dx*dx + dy*dy);

                if (r >= inner_radius && r <= outer_radius) {
                    Color_hsv c_hsv = {0, 0.8, 0.8};
                    c_hsv.h = atan2(dy, dx) * 360.0 / (2 * M_PI);
                    if (c_hsv.h < 0) c_hsv.h += 360;

                    Color c = hsv_to_rgb(c_hsv);
                    uint32 draw_color = (c.A << 24) | (c.R << 16) | (c.G << 8) | c.B;

                    *Pixel = draw_color;
                }
                Pixel++;
            }
            Row += Pitch;
        }
    }

    int cx = cp.x + cp.side/2;
    int cy = cp.y + cp.side/2;

    int qx = cx - 0.39 * cp.side * cos(M_PI / 4);
    int qy = cy - 0.39 * cp.side * sin(M_PI / 4);
    int qside = SQRT_2 * 0.39 * cp.side;

    // Render inner triangle
    {
        int Pitch = Main_Buffer.Width*Bytes_Per_Pixel;
        uint8 *Row = (uint8 *) Main_Buffer.Memory;
        Row += qx*Bytes_Per_Pixel + qy*Pitch;

        for (int y = 0; y < qside; y++) {
            uint32 *Pixel = (uint32 *) Row;
            for (int x = 0; x < qside; x++) {

                Color_hsv c_hsv = cp.current_color_hsv;
                c_hsv.v = (float) x / (float) qside;
                c_hsv.s = (float) y / (float) qside;

                Color c = hsv_to_rgb(c_hsv);
                uint32 draw_color = (c.A << 24) | (c.R << 16) | (c.G << 8) | c.B;

                *Pixel = draw_color;
                Pixel++;
            }
            Row += Pitch;
        }
    }

    // Render markers
    {
        int x = cp.current_color_hsv.v * qside + qx;
        int y = cp.current_color_hsv.s * qside + qy;
        render_rectangle(get_rect(x - 4, y - 4, 8, 8), WHITE, 2);
        
        float theta = cp.current_color_hsv.h - 90;
        if (theta < 0) theta += 360;
        render_circular_angle(WHITE, cx, cy, inner_radius, outer_radius, theta - 3, theta - 1);
        render_circular_angle(WHITE, cx, cy, inner_radius, outer_radius, theta + 1, theta + 3);
    }

    bool hovering = v2_inside_rect(mouse_position, get_rect(qx, qy, qside, qside));
    bool highlighted = hovering && !sliders.pressing_a_slider;

    if (highlighted && left_button_down && !handled_press_left) {
        Color_hsv c = {cp.current_color_hsv.h, 0, 0} ;
        c.v = (float) (mouse_position.x - qx) / (float) (qside);
        c.s = (float) (mouse_position.y - qy) / (float) (qside);

        result = hsv_to_rgb(c);
    }

    float r = sqrt(pow(mouse_position.y - cy, 2) + pow(mouse_position.x - cx, 2));
    if (r < outer_radius && r > inner_radius && left_button_down && !handled_press_left && !sliders.pressing_a_slider) {
        float theta = -atan2(mouse_position.y - cy, mouse_position.x - cx) * 360.0 / (2 * M_PI) + 90;
        if (theta < 0) theta += 360;

        Color_hsv c = cp.current_color_hsv;
        c.h = theta;
        result = hsv_to_rgb(c);
    }

    return result;
}

int main(void) {
    initialize_main_buffer();
    start_main_window();

    int CPM = get_CPM();
    ms t0, t1;
    get_time(&t0);

    memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
    blit_main_buffer_to_window();
    
    int sizes[N_SIZES] = {20, 35, 55};
    init_font(sizes);

    Color initial_colors[4] = {RED, BLUE, GREEN, WHITE};
    int initial_counts[4] = {200, 200, 200, 200};
    init_particles();
    randomize_particles(p, min_x, max_x, min_y, max_y);
    randomize_weights();

    Color_Picker cp;
    cp.bg_color     = GRAY;
    cp.border_color = WHITE;

    while (true) {

        // Handle Messages
        auto result = handle_window_messages();
        if (result == -1) return 0;

        // Handle Inputs        
        POINT pos;
        GetCursorPos(&pos);
        mouse_position = screen_to_window_position(pos);

        // Reset Background
        memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);

        start_sliders()
        int panel_width = 400;
        int panel_x = 20;
        Panel panel = make_panel(panel_x, 5, 30, panel_width, Small_Font);

        panel.row();
        int throwaway = 0;
        if (panel.push_header("Gravities", default_palette, 0, &throwaway)) {
            settings.gravities_visible = !settings.gravities_visible;
        }

        if (settings.gravities_visible) {
            for (int i = 0; i < settings.particle_types; i++) {
                for (int j = 0; j < settings.particle_types; j++) {
                    panel.row();
                    panel.push_slider_with_colors(slider_palette2, &settings.gravities[j + i * settings.particle_types], -50, 50, settings.particle_colors[i], settings.particle_colors[j], new_slider());
                }
            }
        }

        panel.row();
        panel.row();
        panel.push_slider("Gravity", slider_palette, &settings.gravity_divider, 1, 100, new_slider());
        panel.row();
        panel.push_slider("Entropy %", slider_palette, &settings.entropy_scale, 0, 100, new_slider());
        panel.row();
        panel.push_slider("Distance", slider_palette, &settings.max_distance, 10, 600 * SQRT_2, new_slider());
        
        panel.row();
        panel.row();
        if (panel.push_button("Random weights", default_palette, 2) == Button_Left_Clicked) {
            randomize_weights();
            randomize_particles(p, min_x, max_x, min_y, max_y);
        }
        panel.row(2);
        if (panel.push_button("Reset particles", default_palette, 2) == Button_Left_Clicked) {
            randomize_particles(p, min_x, max_x, min_y, max_y);
        }
        if (panel.push_button("Reset weights", default_palette, 2) == Button_Left_Clicked) {
            memset(settings.gravities, 0, sizeof(int) * settings.particle_types * settings.particle_types);
        }

        for (int i = 0; i < settings.particle_types; i++) {
            panel.row(2);
            bool color_pressed = false;
            int result = panel.push_updown_counter(settings.particle_colors[i], default_palette, (void *) &settings.particle_counts[i], &color_pressed, cp.active && cp.color_index == i);

            if (result != 0) {
                settings.particle_counts[i] += 10 * result;
                settings.particle_counts[i] = clamp(settings.particle_counts[i], 0, 1000);
                resize_particle_count();
            }

            if (color_pressed) {
                if (cp.active && cp.color_index == i) {
                    cp.active = false;
                    settings.particle_colors[i] = cp.current_color;
                } else {
                    cp.active = true;
                    cp.color_index = i;
                    cp.side = 300;
                    cp.x = Main_Buffer.Width  / 2 - cp.side / 2;
                    cp.y = Main_Buffer.Height / 2 - cp.side / 2;
                    cp.current_color = settings.particle_colors[i];
                    cp.current_color_hsv = rgb_to_hsv(cp.current_color);
                }
            }

            if (panel.push_button("Delete particles", default_palette) == Button_Left_Clicked) {
                if (settings.particle_types != 1) {
                    remove_particle_type(i);
                }
            }
        }

        panel.row();
        if (panel.push_button("Add new particle", default_palette) == Button_Left_Clicked) {
            add_new_particle_type();
        }

        update_particles();

        render_filled_rectangle(get_rect(panel_width + panel_x, 5, max_x - min_x, max_y - min_y), GRAY);
        for (int i = 0; i < settings.total_particles; i++) {
            if (p[i].x <= min_x || p[i].x >= max_x) continue;
            if (p[i].y <= min_y || p[i].y >= max_y) continue;
            render_filled_rectangle(get_rect(panel_width + panel_x + p[i].x - 2, 5 + p[i].y - 2, 4, 4), settings.particle_colors[p[i].type]); 
        }

        if (cp.active) {
            cp.current_color = click_color_picker(cp);
            cp.current_color_hsv = rgb_to_hsv(cp.current_color);
        }

        handled_press_left  = true;
        handled_press_right = true;
        blit_main_buffer_to_window();

        get_time(&t1);
        int ms_elapsed = time_elapsed(t0, t1, CPM);
        if (ms_elapsed < 10) Sleep(10 - ms_elapsed);
        t0 = t1;
    }
}

void Panel::row(int columns, float height_factor, bool adaptive) {
    if (columns < 1) columns = 1;

    current_row++;
    current_column = 1;
    columns_this_row = columns;

    // First you advance the at_y with the previous row_height
    at_y += row_height;
    at_x  = left_x;

    row_height   = base_height * height_factor;
    column_width = base_width / columns;
    adaptive_row = adaptive;
}

void Panel::empty_row(float height_factor) {
    current_row++;
    current_column = 1;
    columns_this_row = 1;

    // First you advance the at_y with the previous row_height
    at_y += row_height;
    at_x  = left_x;

    row_height   = base_height * height_factor;
    column_width = base_width;
    adaptive_row = false;
}

void Panel::indent(float indent_percentage) {
    int indent_by = indent_percentage * column_width;
    at_x += indent_by;
}

RECT Panel::get_current_rect() {
    return get_rect(at_x, at_y, column_width, row_height);
}

int get_width(int text_length, int font_size) {
    return text_length * (Font.advance * Font.scale[font_size]);
}

bool Panel::push_toggler(char *name, Color_Palette palette, bool *toggled, Color *override_color) {
    int thickness   = 2;
    int rect_side   = row_height - 2 * thickness;
    int inside_side = row_height - 6 * thickness;

    RECT button_rect = get_rect(at_x + thickness,     at_y + thickness,     rect_side, rect_side);
    RECT inside_rect = get_rect(at_x + thickness * 3, at_y + thickness * 3, inside_side, inside_side);

    int name_length = strlen(name);
    int name_width  = column_width - rect_side;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             name_width = left_x + base_width - at_x - rect_side;
             if (name_width < 0) name_width = 0;
         } else {
             name_width = get_width(name_length + 2, font_size);
         }
    }
    RECT name_rect = get_rect(at_x + rect_side, at_y, name_width, row_height);

    bool hovering = v2_inside_rect(mouse_position, button_rect) || v2_inside_rect(mouse_position, name_rect);
    bool highlighted = hovering && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_rectangle(button_rect, button_color, thickness);
    if (override_color) render_filled_rectangle(inside_rect, *override_color);
    else if (*toggled)  render_filled_rectangle(inside_rect, button_color);
    render_text(name, name_length, font_size, name_rect, text_color);

    int toggler_width = rect_side + name_width;
    at_x += toggler_width;
    current_column++;

    if (highlighted && left_button_down && !handled_press_left) {
        *toggled = !(*toggled);
        return true;
    }
    return false;
}

int Panel::push_updown_counter(char *name, Color_Palette palette, void *value, bool is_float) {
    if (adaptive_row) assert(false);
    int thickness = 2;
    int rect_side = row_height - 2 * thickness;

    int name_length = strlen(name);

    RECT minus_rect = get_rect(at_x + thickness, at_y + thickness, rect_side,     rect_side);
    RECT value_rect = get_rect(minus_rect.right, at_y,             rect_side * 3, rect_side);
    RECT plus_rect  = get_rect(value_rect.right, at_y + thickness, rect_side,     rect_side);
    RECT name_rect  = get_rect(plus_rect.right,  at_y, (Font.advance * Font.scale[font_size]) * (name_length + 2), rect_side);

    bool highlighted = v2_inside_rect(mouse_position, get_current_rect()) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    char text[2] = "+";
    render_text(text, 1, font_size, plus_rect,  button_color);
    text[0] = '-';
    render_text(text, 1, font_size, minus_rect, button_color);

    render_rectangle(minus_rect, button_color, thickness);
    render_rectangle(plus_rect,  button_color, thickness);
    render_text(name, name_length, font_size, name_rect, text_color);

    if (is_float) render_text(*(float *)value, 2, font_size, value_rect, value_color);
    else          render_text(*(int *)  value,    font_size, value_rect, value_color);

    at_x += column_width;
    current_column++;

    if (left_button_down && !handled_press_left) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -1;
        if (v2_inside_rect(mouse_position, plus_rect))  return +1;
    }

    if (right_button_down && !handled_press_right) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -10;
        if (v2_inside_rect(mouse_position, plus_rect))  return +10;
    }

    return 0;
}

int Panel::push_updown_counter(Color c, Color_Palette palette, void *value, bool *color_pressed, bool highlight_border) {
    if (adaptive_row) assert(false);
    int thickness = 2;
    int rect_side = row_height - 2 * thickness;

    RECT minus_rect = get_rect(at_x + thickness, at_y + thickness, rect_side,     rect_side);
    RECT value_rect = get_rect(minus_rect.right, at_y,             rect_side * 3, rect_side);
    RECT plus_rect  = get_rect(value_rect.right, at_y + thickness, rect_side,     rect_side);
    RECT color_rect = get_rect(plus_rect.right + thickness * 2,  at_y + thickness, column_width - plus_rect.right + thickness * 6, rect_side);

    bool highlighted = v2_inside_rect(mouse_position, get_current_rect()) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    char text[2] = "+";
    render_text(text, 1, font_size, plus_rect,  button_color);
    text[0] = '-';
    render_text(text, 1, font_size, minus_rect, button_color);

    render_rectangle(minus_rect, button_color, thickness);
    render_rectangle(plus_rect,  button_color, thickness);
    render_filled_rectangle(color_rect, c);

    if (highlight_border) render_rectangle(color_rect, WHITE, thickness);

    render_text(*(int *) value, font_size, value_rect, value_color);

    at_x += column_width;
    current_column++;

    *color_pressed = false;
    if (left_button_down && !handled_press_left) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -1;
        if (v2_inside_rect(mouse_position, plus_rect))  return +1;
        if (v2_inside_rect(mouse_position, color_rect)) *color_pressed = true;
    }

    if (right_button_down && !handled_press_right) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -10;
        if (v2_inside_rect(mouse_position, plus_rect))  return +10;
        if (v2_inside_rect(mouse_position, color_rect)) *color_pressed = true;
    }

    return 0;
}

void Panel::add_text(char *text, Color c, int override_font_size) {
    int text_font_size = override_font_size != -1 ? override_font_size : font_size;
    
    int text_length = strlen(text);
    int text_width = column_width;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             text_width = left_x + base_width - at_x;
             if (text_width < 0) text_width = 0;
         } else {
             text_width = get_width(text_length + 2, text_font_size);
         }
    }
    RECT text_rect = get_rect(at_x, at_y, text_width, row_height);
    render_text(text, text_length, text_font_size, text_rect, c);

    at_x += text_width;
    current_column++;
}

bool Panel::push_header(char *text, Color_Palette palette, int header, int *current_header) {

    int text_length = strlen(text);
    int text_width = column_width;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             text_width = left_x + base_width - at_x;
             if (text_width < 0) text_width = 0;
         } else {
             text_width = get_width(text_length + 2, font_size);
         }
    }

    RECT text_rect = get_rect(at_x, at_y, text_width, row_height);
    bool highlighted = v2_inside_rect(mouse_position, text_rect) && !sliders.pressing_a_slider;

    Color text_color = palette.text_color;
    Color background_color = palette.background_color;
    if (header == (*current_header)) {
        background_color = palette.highlight_button_color;
        text_color = palette.highlight_text_color;
    } else if (highlighted) {
        background_color = palette.button_color;
        text_color = palette.highlight_text_color;
    }
    
    render_filled_rectangle(text_rect, background_color);
    render_text(text, strlen(text), font_size, text_rect, text_color);

    at_x += text_width;
    current_column++;

    if (highlighted && left_button_down && !handled_press_left) {
        if (*current_header == header) return true;
       *current_header = header;
    }
    return false;
}

int Panel::push_selector(char *text, Color_Palette palette) {
    if (adaptive_row) assert(false);
    int thickness = 2;
    int rect_side = row_height - 2 * thickness;

    RECT at = get_current_rect();
    RECT minus_rect = get_rect(at_x + thickness, at_y + thickness, rect_side, rect_side);
    RECT plus_rect  = get_rect(at.right - rect_side - thickness, at_y + thickness, rect_side, rect_side);
    RECT text_rect  = get_rect(minus_rect.right, at_y, plus_rect.left - minus_rect.right, row_height);

    bool highlighted = v2_inside_rect(mouse_position, at) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    char sign[2] = "<";
    render_text(sign, 1, font_size, minus_rect, button_color);
    sign[0] = '>';
    render_text(sign, 1, font_size, plus_rect,  button_color);

    render_rectangle(minus_rect, button_color, thickness);
    render_rectangle(plus_rect,  button_color, thickness);
    render_text(text, strlen(text), font_size, text_rect, text_color);
    
    at_x += column_width;
    current_column++;

    if (left_button_down && !handled_press_left) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -1;
        if (v2_inside_rect(mouse_position, plus_rect))  return +1;
    }

    return 0;
}

int Panel::push_button(char *text, Color_Palette palette, int thickness) {

    int text_length = strlen(text);
    int width = column_width;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             width = left_x + base_width - at_x;
             if (width < 0) width = 0;
         } else {
             width = get_width(text_length + 2, font_size);
         }
    }

    RECT button_rect = get_rect(at_x, at_y, width, row_height);
    button_rect.left   += thickness;
    button_rect.right  -= thickness;
    button_rect.top    += thickness;
    button_rect.bottom -= thickness;

    bool highlighted = v2_inside_rect(mouse_position, button_rect) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(button_rect, palette.background_color);
    render_rectangle(button_rect, button_color, thickness);
    render_text(text, text_length, font_size, button_rect, text_color);

    at_x += column_width;
    current_column++;

    if (left_button_down  && !handled_press_left  && highlighted) return Button_Left_Clicked;
    if (right_button_down && !handled_press_right && highlighted) return Button_Right_Clicked;
    if (highlighted)                                              return Button_Hovered;

    return Button_not_Pressed_nor_Hovered;
}

bool slider_is_pressed(int index) {
    if (!sliders.pressing_a_slider)               return false;
    if (sliders.which_slider_is_pressed != index) return false;
    return true;
}
bool slider_is_pressed(int index, bool high) {
    if (!sliders.pressing_a_slider)               return false;
    if (sliders.which_slider_is_pressed != index) return false;
    if (sliders.high_slider_pressed     != high)  return false;

    return true;
}

bool Panel::push_slider(char *text, Color_Palette palette, int *value, int min_v, int max_v, int slider_order) {
    if (adaptive_row) assert(false);
    int text_length = strlen(text);
    int initial_value = *value;

    RECT slider_rect = get_rect(at_x,                    at_y, column_width / 2, row_height);
    RECT text_rect   = get_rect(at_x + column_width / 2, at_y, column_width / 2, row_height);
    
    int  thickness   = 2;
    int  slider_side = get_h(slider_rect) - thickness * 2;
    RECT line_rect = get_rect(slider_rect.left, (slider_rect.top + slider_rect.bottom - thickness) / 2, get_w(slider_rect), thickness);

    float pos = (float) (*value - min_v) / (float) (max_v - min_v);
    int slider_position = slider_rect.left + (get_w(slider_rect) - slider_side) * pos;
    RECT pos_rect = get_rect(slider_position, slider_rect.top + thickness, slider_side, slider_side);
    
    bool highlighted = (v2_inside_rect(mouse_position, get_current_rect()) || slider_is_pressed(slider_order)) && !(left_button_down && handled_press_left);
    if (sliders.pressing_a_slider && !slider_is_pressed(slider_order)) highlighted = false;
    
    Color slider_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(line_rect, palette.button_color);
    render_filled_rectangle(pos_rect,  slider_color);
    render_text(text, text_length, font_size, text_rect, text_color);
    
    if (!left_button_down) sliders.pressing_a_slider = false;
    else if (sliders.pressing_a_slider) {
        if (slider_is_pressed(slider_order)) {
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        }
    } else if (!handled_press_left) {
        if (v2_inside_rect(mouse_position, slider_rect)) {
            sliders.pressing_a_slider = true;
            sliders.which_slider_is_pressed = slider_order;
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        } else if (v2_inside_rect(mouse_position, text_rect)) {
            *value = (max_v + min_v) / 2;
        }
    }

    render_text(*value, Small_Font, pos_rect, value_color);

    at_x += column_width;
    current_column++;

    return initial_value != *value;
}

bool Panel::push_slider_with_colors(Color_Palette palette, int *value, int min_v, int max_v, Color c0, Color c1, int slider_order) {
    if (adaptive_row) assert(false);
    int initial_value = *value;

    int thickness = 2;
    int side = row_height - thickness * 2;
    RECT slider_rect  = get_rect(at_x, at_y, column_width * 0.6, row_height);
    RECT color_rect_0 = get_rect(at_x + column_width * 0.65, at_y + thickness, column_width * 0.05, side);
    RECT text_rect    = get_rect(at_x + column_width * 0.7,  at_y, column_width * 0.2,  row_height);
    RECT color_rect_1 = get_rect(at_x + column_width * 0.9,  at_y + thickness, column_width * 0.05, side);
    
    RECT line_rect = get_rect(slider_rect.left, (slider_rect.top + slider_rect.bottom - thickness) / 2, get_w(slider_rect), thickness);

    float pos = (float) (*value - min_v) / (float) (max_v - min_v);
    int slider_position = slider_rect.left + get_w(slider_rect) * pos;
    int slider_side = get_h(slider_rect) - thickness * 2;
    RECT pos_rect = get_rect(slider_position - thickness, slider_rect.top + thickness, thickness * 2, slider_side);
    
    bool highlighted = (v2_inside_rect(mouse_position, get_current_rect()) || slider_is_pressed(slider_order)) && !(left_button_down && handled_press_left);
    if (sliders.pressing_a_slider && !slider_is_pressed(slider_order)) highlighted = false;
    
    Color slider_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(line_rect, palette.button_color);
    render_filled_rectangle(pos_rect,  slider_color);
    
    if (!left_button_down) sliders.pressing_a_slider = false;
    else if (sliders.pressing_a_slider) {
        if (slider_is_pressed(slider_order)) {
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        }
    } else if (!handled_press_left) {
        if (v2_inside_rect(mouse_position, slider_rect)) {
            sliders.pressing_a_slider = true;
            sliders.which_slider_is_pressed = slider_order;
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        } else if (v2_inside_rect(mouse_position, text_rect)) {
            *value = (max_v + min_v) / 2;
        }
    }

    render_filled_rectangle(color_rect_0, c0);
    render_text(*value, Small_Font, text_rect, value_color);
    render_filled_rectangle(color_rect_1, c1);

    at_x += column_width;
    current_column++;

    return initial_value != *value;
}

bool Panel::push_double_slider(char *text, Color_Palette palette, int *low_value, int *high_value, int min_v, int max_v, int slider_order, int extra_value) {
    if (adaptive_row) assert(false);
    int initial_high_value = *high_value;
    int initial_low_value  = *low_value;

    int text_length  = strlen(text);
    int text_width   = (text_length + 2) * Font.advance * Font.scale[font_size];
    int slider_width = column_width - text_width;

    RECT slider_rect = get_rect(at_x,                at_y, slider_width, row_height);
    RECT text_rect   = get_rect(at_x + slider_width, at_y, text_width,   row_height);

    bool highlighted = v2_inside_rect(mouse_position, get_current_rect()) && !(left_button_down && handled_press_left);
    
    int thickness = 2;
    int slider_height    = get_h(slider_rect) - thickness * 2;
    int slider_thickness = slider_height * 0.3;
    RECT line_rect = get_rect(slider_rect.left, (slider_rect.top + slider_rect.bottom - thickness) / 2, get_w(slider_rect), thickness);
    render_filled_rectangle(line_rect, palette.button_color);

    float high_pos = (float) (*high_value - min_v) / (float) (max_v - min_v);
    float low_pos  = (float) (*low_value  - min_v) / (float) (max_v - min_v);
    int high_slider_position = slider_rect.left + (get_w(slider_rect) - slider_thickness) * high_pos;
    int low_slider_position  = slider_rect.left + (get_w(slider_rect) - slider_thickness) * low_pos;

    if (extra_value > min_v) {
        float extra_pos = (float) (extra_value - min_v) / (float) (max_v - min_v);
        int extra_slider_position = slider_rect.left + (get_w(slider_rect) - slider_thickness) * extra_pos;
        RECT extra_pos_rect = get_rect(extra_slider_position, slider_rect.top + thickness, slider_thickness, slider_height);
        render_filled_rectangle(extra_pos_rect, palette.text_color);
    }

    RECT high_pos_rect = get_rect(high_slider_position, slider_rect.top + thickness, slider_thickness, slider_height);
    RECT low_pos_rect  = get_rect(low_slider_position,  slider_rect.top + thickness, slider_thickness, slider_height);

    Color high_slider_color = palette.button_color;
    Color low_slider_color  = palette.button_color;
    if (slider_is_pressed(slider_order,  true)) high_slider_color = palette.highlight_button_color;
    if (slider_is_pressed(slider_order, false)) low_slider_color  = palette.highlight_button_color;

    float value_0_to_1 = clamp((float) (mouse_position.x - slider_rect.left) / get_w(slider_rect), 0, 1);
    int value = min_v + value_0_to_1 * (max_v - min_v);
    bool high_is_closer = abs(value - *low_value) > abs(value - *high_value);

    if (!sliders.pressing_a_slider && highlighted) {
        if (high_is_closer) high_slider_color = palette.highlight_button_color;
        else                low_slider_color  = palette.highlight_button_color;
    }
    render_filled_rectangle(high_pos_rect, high_slider_color);
    render_filled_rectangle(low_pos_rect,  low_slider_color);
    
    Color text_color = highlighted || slider_is_pressed(slider_order) ? palette.highlight_text_color : palette.text_color;
    render_text(text, text_length, font_size, text_rect, text_color);

    if (!left_button_down) {
        sliders.pressing_a_slider = false;
    } else if (sliders.pressing_a_slider) {
        if (sliders.which_slider_is_pressed == slider_order) {
            if (sliders.high_slider_pressed) {
                *high_value = value;
                *high_value = MAX(*high_value, *low_value);
            } else {
                *low_value = value;
                *low_value = MIN(*low_value, *high_value);
            }
        }
    } else if (!handled_press_left) {
        if (v2_inside_rect(mouse_position, slider_rect)) {
            sliders.pressing_a_slider = true;
            sliders.which_slider_is_pressed = slider_order;
            if (high_is_closer) {
                sliders.high_slider_pressed = true;
                *high_value = value;
            } else {
                sliders.high_slider_pressed = false;
                *low_value = value;
            }
        } else if (v2_inside_rect(mouse_position, text_rect)) {
            *high_value = max_v;
            *low_value  = min_v;
        }
    }

    at_x += column_width;
    current_column++;

    return (initial_high_value != *high_value || initial_low_value != *low_value);
}