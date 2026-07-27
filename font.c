// font.c — Font support for text mode (placeholder for future pixel GUI)

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

static unsigned char font[256][8] = {
    // All zeros placeholder — will be replaced with actual font
};

unsigned char* get_font_glyph(unsigned char c) {
    return font[c];
}
