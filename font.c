// font.c — Font support for text mode (placeholder for future pixel GUI)

// Freestanding type definitions
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

// Simple 8x8 font bitmap (placeholder)
static unsigned char font[256][8] = {
    // All zeros placeholder — will be replaced with actual font
};

// Get font glyph for character
unsigned char* get_font_glyph(unsigned char c) {
    return font[c];
}

// Draw character in text mode (VGA)
void draw_char_textmode(unsigned char c, int x, int y, uint32_t fg, uint32_t bg) {
    // In text mode, we use VGA memory directly via PutCharAt
    // This function is a placeholder for future pixel-based GUI
    (void)c;
    (void)x;
    (void)y;
    (void)fg;
    (void)bg;
}