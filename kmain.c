// kmain.c — IncedenaryOS with FAT, Keyboard, Shell, and Scrolling

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// ============================================
// Standard Library Functions (Freestanding)
// ============================================

void* memcpy(void* dest, const void* src, unsigned int n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (unsigned int i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

// ============================================
// VGA Text Mode Functions
// ============================================

enum vga_color {
    COLOR_BLACK         = 0,
    COLOR_BLUE          = 1,
    COLOR_GREEN         = 2,
    COLOR_CYAN          = 3,
    COLOR_RED           = 4,
    COLOR_MAGENTA       = 5,
    COLOR_BROWN         = 6,
    COLOR_LIGHT_GREY    = 7,
    COLOR_DARK_GREY     = 8,
    COLOR_LIGHT_BLUE    = 9,
    COLOR_LIGHT_GREEN   = 10,
    COLOR_LIGHT_CYAN    = 11,
    COLOR_LIGHT_RED     = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_LIGHT_BROWN   = 14,
    COLOR_WHITE         = 15,
    COLOR_YELLOW        = 14
};

// Global cursor position for scrolling
static int cursor_x = 0;
static int cursor_y = 0;

// Forward declaration of scroll_screen
void scroll_screen(void);

void PutCharAt(char c, int x, int y, enum vga_color fg, enum vga_color bg) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT || x < 0 || y < 0) {
        return;
    }
    unsigned short* vga = (unsigned short*) VGA_MEMORY;
    unsigned short color = (bg << 4) | fg;
    vga[y * VGA_WIDTH + x] = (color << 8) | c;
}

void Print(const char* str, enum vga_color fg, enum vga_color bg) {
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            PutCharAt(str[i], cursor_x, cursor_y, fg, bg);
            cursor_x++;
            if (cursor_x >= VGA_WIDTH) {
                cursor_x = 0;
                cursor_y++;
            }
        }
        i++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll_screen();
            cursor_y = VGA_HEIGHT - 1;
        }
    }
}

void PrintLn(const char* str, enum vga_color fg, enum vga_color bg) {
    Print(str, fg, bg);
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT) {
        scroll_screen();
        cursor_y = VGA_HEIGHT - 1;
    }
}

void scroll_screen(void) {
    unsigned short* vga = (unsigned short*) VGA_MEMORY;
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        PutCharAt(' ', x, VGA_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
    }
}

void ClearScreen(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            PutCharAt(' ', x, y, COLOR_BLACK, COLOR_BLACK);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

void PrintHex(unsigned int value) {
    const char hex[] = "0123456789ABCDEF";
    char buffer[9];
    for (int i = 0; i < 8; i++) buffer[i] = '0';
    buffer[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex[value & 0xF];
        value >>= 4;
    }
    Print(buffer, COLOR_LIGHT_CYAN, COLOR_BLACK);
}

// ============================================
// Keyboard Driver (Polling)
// ============================================

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define BUFFER_SIZE 256

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "d"(port));
    return result;
}

void outb(unsigned short port, unsigned char data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "d"(port));
}

char scancode_to_char(unsigned char scancode) {
    static int shift_pressed = 0;
    if (scancode & 0x80) {
        if (scancode == 0xAA || scancode == 0xB6) shift_pressed = 0;
        return 0;
    }
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    char c = scancode_to_ascii[scancode];
    if (shift_pressed && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    return c;
}

char get_key(void) {
    while (1) {
        if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
            unsigned char scancode = inb(KEYBOARD_DATA_PORT);
            // Handle special keys
            if (scancode == 0x1C) return '\n';      // Enter
            if (scancode == 0x0E) return '\b';      // Backspace
            char c = scancode_to_char(scancode);
            if (c) return c;
        }
    }
}

// ============================================
// FAT Filesystem Structures
// ============================================

#pragma pack(push, 1)
typedef struct {
    unsigned char  jump[3];
    unsigned char  oem[8];
    unsigned short bytes_per_sector;
    unsigned char  sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char  fat_count;
    unsigned short root_entries;
    unsigned short total_sectors_16;
    unsigned char  media_descriptor;
    unsigned short fat_size_16;
    unsigned short sectors_per_track;
    unsigned short head_count;
    unsigned int   hidden_sectors;
    unsigned int   total_sectors_32;
    unsigned char  drive_number;
    unsigned char  reserved;
    unsigned char  boot_signature;
    unsigned int   volume_id;
    unsigned char  volume_label[11];
    unsigned char  filesystem_type[8];
    unsigned char  boot_code[448];
    unsigned short signature;
} __attribute__((packed)) FAT_BootSector;

typedef struct {
    unsigned char  filename[8];
    unsigned char  ext[3];
    unsigned char  attributes;
    unsigned char  reserved;
    unsigned char  creation_time_tenths;
    unsigned short creation_time;
    unsigned short creation_date;
    unsigned short last_access_date;
    unsigned short first_cluster_high;
    unsigned short last_mod_time;
    unsigned short last_mod_date;
    unsigned short first_cluster_low;
    unsigned int   file_size;
} __attribute__((packed)) FAT_DirectoryEntry;
#pragma pack(pop)

void ReadSector(unsigned int sector, unsigned char* buffer) {
    (void)buffer;
    Print("Reading sector: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintHex(sector);
    Print("\n", COLOR_WHITE, COLOR_BLACK);
}

int DetectFAT(FAT_BootSector* boot) {
    if (boot->signature != 0xAA55) return 0;
    char* fs_type = (char*)boot->filesystem_type;
    if (fs_type[0] == 'F' && fs_type[1] == 'A' && fs_type[2] == 'T') return 1;
    return 0;
}

void ReadRootDirectory(FAT_BootSector* boot, unsigned char* disk_data) {
    unsigned int root_dir_start = boot->reserved_sectors + (boot->fat_count * boot->fat_size_16);
    Print("Root directory at sector: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintHex(root_dir_start);
    Print("\n", COLOR_WHITE, COLOR_BLACK);
    
    FAT_DirectoryEntry* entries = (FAT_DirectoryEntry*)(disk_data + root_dir_start * boot->bytes_per_sector);
    Print("Files found:\n", COLOR_LIGHT_GREEN, COLOR_BLACK);
    
    for (int i = 0; i < boot->root_entries; i++) {
        if (entries[i].filename[0] == 0x00) break;
        if (entries[i].filename[0] == 0xE5) continue;
        
        char name[13];
        int idx = 0;
        for (int j = 0; j < 8 && entries[i].filename[j] != ' '; j++) {
            name[idx++] = entries[i].filename[j];
        }
        if (entries[i].ext[0] != ' ') {
            name[idx++] = '.';
            for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++) {
                name[idx++] = entries[i].ext[j];
            }
        }
        name[idx] = '\0';
        Print("  ", COLOR_WHITE, COLOR_BLACK);
        Print(name, COLOR_WHITE, COLOR_BLACK);
        Print("  (", COLOR_DARK_GREY, COLOR_BLACK);
        char size_str[12];
        int size = entries[i].file_size;
        size_str[11] = '\0';
        for (int j = 10; j >= 0; j--) {
            if (size > 0 || j == 10) {
                size_str[j] = '0' + (size % 10);
                size /= 10;
            } else {
                size_str[j] = ' ';
            }
        }
        Print(size_str, COLOR_LIGHT_CYAN, COLOR_BLACK);
        Print(" bytes)\n", COLOR_DARK_GREY, COLOR_BLACK);
    }
}

// ============================================
// Commands
// ============================================

void cmd_help(void) {
    PrintLn("Available commands:", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  help     - Show this help", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  echo     - Echo text back", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  clear    - Clear screen", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  reboot   - Reboot system", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  hexdump  - Dump memory", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  ls       - List files", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  grace    - Display Grace message", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  print    - Print text (alias for echo)", COLOR_WHITE, COLOR_BLACK);
}

void cmd_echo(void) {
    PrintLn("Echo!", COLOR_LIGHT_CYAN, COLOR_BLACK);
}

void cmd_print(void) {
    PrintLn("Print!", COLOR_LIGHT_CYAN, COLOR_BLACK);
}

void cmd_clear(void) {
    ClearScreen();
    PrintLn("Screen cleared. Type 'help' for commands.", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_reboot(void) {
    PrintLn("Rebooting...", COLOR_LIGHT_RED, COLOR_BLACK);
    for (volatile int i = 0; i < 10000000; i++);
    __asm__ volatile (
        "mov $0x64, %%al\n"
        "out %%al, $0x64\n"
        "cli\n"
        "hlt\n"
        : : : "eax", "memory"
    );
}

void cmd_hexdump(void) {
    PrintLn("Hexdump of kernel memory (0x100000):", COLOR_LIGHT_GREY, COLOR_BLACK);
    unsigned char* ptr = (unsigned char*)0x100000;
    for (int row = 0; row < 8; row++) {
        char hex_str[3];
        for (int i = 0; i < 16; i++) {
            unsigned char val = ptr[row * 16 + i];
            hex_str[0] = "0123456789ABCDEF"[(val >> 4) & 0xF];
            hex_str[1] = "0123456789ABCDEF"[val & 0xF];
            hex_str[2] = '\0';
            Print(hex_str, COLOR_LIGHT_CYAN, COLOR_BLACK);
            Print(" ", COLOR_WHITE, COLOR_BLACK);
        }
        Print("\n", COLOR_WHITE, COLOR_BLACK);
    }
}

void cmd_ls(void) {
    PrintLn("Files on disk:", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  README.TXT    (1024 bytes)", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  KERNEL.BIN    (16384 bytes)", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  GRACE.TXT     (512 bytes)", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  FERRITE.ENG   (314159 bytes)", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
}

void cmd_grace(void) {
    PrintLn("   .---.  .---.     .---.     .-.     .---. ", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn("  / .-. \\/ .-. \\   / .-. \\   / _ \\   / .-. \\", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | /   || /   |  | /   |  | / \\ | | /   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" | |   || |   |  | |   |  | | | | | |   |", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn(" '---'  '---'   '---'   '---'   '---'   '---'", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  For Grace - the reason this OS exists. ", COLOR_LIGHT_CYAN, COLOR_BLACK);
}

// ============================================
// Command Table
// ============================================

typedef struct {
    const char* name;
    void (*func)(void);
} cmd_entry_t;

cmd_entry_t cmd_table[] = {
    {"help",    cmd_help},
    {"echo",    cmd_echo},
    {"clear",   cmd_clear},
    {"reboot",  cmd_reboot},
    {"hexdump", cmd_hexdump},
    {"ls",      cmd_ls},
    {"grace",   cmd_grace},
    {"print",   cmd_print}
};

int cmd_count = sizeof(cmd_table) / sizeof(cmd_entry_t);

// ============================================
// Shell
// ============================================

void shell(void) {
    char input[BUFFER_SIZE];
    int input_len = 0;
    ClearScreen();
    PrintLn("IncedenaryOS v2026.8 - Shell", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Type 'help' for commands", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    while (1) {
        Print("> ", COLOR_LIGHT_GREEN, COLOR_BLACK);
        input_len = 0;
        
        while (1) {
            char c = get_key();
            
            if (c == '\n') {
                // Enter key — submit command
                Print("\n", COLOR_WHITE, COLOR_BLACK);
                break;
            } else if (c == '\b') {
                // Backspace — remove last character
                if (input_len > 0) {
                    input_len--;
                    cursor_x--;
                    PutCharAt(' ', cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
                }
            } else if (c >= 32 && c <= 126 && input_len < BUFFER_SIZE - 1) {
                // Printable character
                input[input_len++] = c;
                PutCharAt(c, cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
                cursor_x++;
                if (cursor_x >= VGA_WIDTH) {
                    cursor_x = 0;
                    cursor_y++;
                    if (cursor_y >= VGA_HEIGHT) scroll_screen();
                }
            }
        }
        
        // Null-terminate the input
        input[input_len] = '\0';
        
        // Execute command
        int found = 0;
        for (int i = 0; i < cmd_count; i++) {
            if (strcmp(input, cmd_table[i].name) == 0) {
                cmd_table[i].func();
                found = 1;
                break;
            }
        }
        if (!found && input[0] != '\0') {
            Print("Unknown command: ", COLOR_LIGHT_RED, COLOR_BLACK);
            PrintLn(input, COLOR_LIGHT_RED, COLOR_BLACK);
        }
        PrintLn("", COLOR_WHITE, COLOR_BLACK);
    }
}

// ============================================
// Kernel Entry Point
// ============================================

void kmain(void) {
    ClearScreen();
    PrintLn("IncedenaryOS v2026.8", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Kernel loaded at 0x100000", COLOR_CYAN, COLOR_BLACK);
    PrintLn("Kernel stack size: 16384 bytes", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    // FAT demo
    PrintLn("Initializing FAT filesystem...", COLOR_LIGHT_GREY, COLOR_BLACK);
    unsigned char disk_data[1024 * 1024];
    FAT_BootSector* boot = (FAT_BootSector*)disk_data;
    boot->signature = 0xAA55;
    char fs_type[] = "FAT12   ";
    for (int i = 0; i < 8; i++) boot->filesystem_type[i] = fs_type[i];
    boot->bytes_per_sector = 512;
    boot->sectors_per_cluster = 1;
    boot->reserved_sectors = 1;
    boot->fat_count = 2;
    boot->root_entries = 224;
    boot->total_sectors_16 = 2880;
    boot->fat_size_16 = 9;
    
    FAT_DirectoryEntry* entries = (FAT_DirectoryEntry*)(disk_data + (boot->reserved_sectors + boot->fat_count * boot->fat_size_16) * 512);
    
    char filename1[] = "README";
    for (int i = 0; i < 8; i++) entries[0].filename[i] = (i < 6) ? filename1[i] : ' ';
    char ext1[] = "TXT";
    for (int i = 0; i < 3; i++) entries[0].ext[i] = ext1[i];
    entries[0].file_size = 1024;
    
    char filename2[] = "KERNEL";
    for (int i = 0; i < 8; i++) entries[1].filename[i] = (i < 6) ? filename2[i] : ' ';
    char ext2[] = "BIN";
    for (int i = 0; i < 3; i++) entries[1].ext[i] = ext2[i];
    entries[1].file_size = 16384;
    
    char filename3[] = "GRACE";
    for (int i = 0; i < 8; i++) entries[2].filename[i] = (i < 5) ? filename3[i] : ' ';
    char ext3[] = "TXT";
    for (int i = 0; i < 3; i++) entries[2].ext[i] = ext3[i];
    entries[2].file_size = 512;
    
    if (DetectFAT(boot)) {
        PrintLn("FAT filesystem detected!", COLOR_LIGHT_GREEN, COLOR_BLACK);
        ReadRootDirectory(boot, disk_data);
    } else {
        PrintLn("No FAT filesystem found.", COLOR_LIGHT_RED, COLOR_BLACK);
    }
    
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    PrintLn("Starting shell...", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    shell();
}