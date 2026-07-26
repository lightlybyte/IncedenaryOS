// kmain.c — IncedenaryOS with POSIX commands, scrollback, and memory tracking

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define NULL ((void*)0)

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

int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
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

static int cursor_x = 0;
static int cursor_y = 0;
static int terminal_rows = VGA_HEIGHT - 2;

void scroll_screen(void);

void PutCharAt(char c, int x, int y, enum vga_color fg, enum vga_color bg) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT || x < 0 || y < 0) return;
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
        if (cursor_y >= VGA_HEIGHT - 1) {
            scroll_screen();
            cursor_y = VGA_HEIGHT - 2;
        }
    }
}

// ============================================
// Scrollback Buffer
// ============================================

#define SCROLLBACK_LINES 256
#define SCROLLBACK_WIDTH VGA_WIDTH

static char scrollback_buffer[SCROLLBACK_LINES][SCROLLBACK_WIDTH];
static int scrollback_head = 0;
static int scrollback_count = 0;
static int scroll_offset = 0;
static int in_scroll_mode = 0;

void scrollback_add_line(const char* str) {
    int len = strlen(str);
    if (len > SCROLLBACK_WIDTH - 1) len = SCROLLBACK_WIDTH - 1;
    
    for (int i = 0; i < SCROLLBACK_WIDTH; i++) {
        scrollback_buffer[scrollback_head][i] = (i < len) ? str[i] : ' ';
    }
    scrollback_buffer[scrollback_head][SCROLLBACK_WIDTH - 1] = '\0';
    
    scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
    if (scrollback_count < SCROLLBACK_LINES) {
        scrollback_count++;
    }
    if (scroll_offset > 0) scroll_offset++;
}

void scrollback_display(void) {
    ClearScreen();
    int display_lines = VGA_HEIGHT - 2;
    int start = (scrollback_head - scroll_offset - display_lines);
    if (start < 0) start += SCROLLBACK_LINES;
    
    for (int i = 0; i < display_lines && i < scrollback_count; i++) {
        int idx = (start + i) % SCROLLBACK_LINES;
        Print(scrollback_buffer[idx], COLOR_WHITE, COLOR_BLACK);
        Print("\n", COLOR_WHITE, COLOR_BLACK);
    }
    
    Print("--- Scroll mode (PgUp/PgDown, ESC to exit) ---", COLOR_LIGHT_GREY, COLOR_BLACK);
}

// ============================================
// Memory Tracking
// ============================================

static unsigned int total_memory_kb = 16384;
static unsigned int used_memory_kb = 0;
static unsigned int peak_memory_kb = 0;

void memory_alloc(unsigned int size_kb) {
    used_memory_kb += size_kb;
    if (used_memory_kb > peak_memory_kb) {
        peak_memory_kb = used_memory_kb;
    }
}

void memory_free(unsigned int size_kb) {
    if (used_memory_kb >= size_kb) {
        used_memory_kb -= size_kb;
    } else {
        used_memory_kb = 0;
    }
}

// ============================================
// PrintLn with scrollback
// ============================================

void PrintLn(const char* str, enum vga_color fg, enum vga_color bg) {
    Print(str, fg, bg);
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT - 1) {
        scroll_screen();
        cursor_y = VGA_HEIGHT - 2;
    }
    scrollback_add_line(str);
}

void scroll_screen(void) {
    unsigned short* vga = (unsigned short*) VGA_MEMORY;
    for (int y = 1; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        PutCharAt(' ', x, VGA_HEIGHT - 2, COLOR_BLACK, COLOR_BLACK);
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
// Keyboard Driver
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
            if (scancode == 0x01) return 0x1B;    // Escape
            if (scancode == 0x1C) return '\n';    // Enter
            if (scancode == 0x0E) return '\b';    // Backspace
            if (scancode == 0x49) return 0x49;    // Page Up
            if (scancode == 0x51) return 0x51;    // Page Down
            char c = scancode_to_char(scancode);
            if (c) return c;
        }
    }
}

// ============================================
// FAT Structures (Simulated)
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

static char current_dir[64] = "/";

// ============================================
// POSIX Command Implementations (Forward Declarations)
// ============================================

void cmd_help(const char* args);
void cmd_echo(const char* args);
void cmd_clear(const char* args);
void cmd_reboot(const char* args);
void cmd_hexdump(const char* args);
void cmd_ls(const char* args);
void cmd_pwd(const char* args);
void cmd_cd(const char* args);
void cmd_exit(const char* args);
void cmd_touch(const char* args);
void cmd_cat(const char* args);
void cmd_mkdir(const char* args);
void cmd_rm(const char* args);
void cmd_rmdir(const char* args);
void cmd_cp(const char* args);
void cmd_mv(const char* args);
void cmd_grep(const char* args);
void cmd_head(const char* args);
void cmd_tail(const char* args);
void cmd_wc(const char* args);
void cmd_uname(const char* args);
void cmd_uptime(const char* args);
void cmd_free(const char* args);
void cmd_df(const char* args);
void cmd_du(const char* args);
void cmd_date(const char* args);
void cmd_whoami(const char* args);
void cmd_sleep(const char* args);
void cmd_editor(const char* filename);
void cmd_nano(const char* args);
void cmd_vim(const char* args);
void cmd_memory(const char* args);

// ============================================
// Command Table
// ============================================

typedef struct {
    const char* name;
    void (*func)(const char* args);
    const char* description;
} cmd_entry_t;

cmd_entry_t cmd_table[] = {
    {"help",    cmd_help,    "Show available commands"},
    {"echo",    cmd_echo,    "Print text"},
    {"clear",   cmd_clear,   "Clear the screen"},
    {"exit",    cmd_exit,    "Exit the shell"},
    {"cd",      cmd_cd,      "Change directory"},
    {"pwd",     cmd_pwd,     "Print working directory"},
    {"ls",      cmd_ls,      "List directory contents"},
    {"mkdir",   cmd_mkdir,   "Create directory"},
    {"rmdir",   cmd_rmdir,   "Remove empty directory"},
    {"rm",      cmd_rm,      "Remove files"},
    {"cp",      cmd_cp,      "Copy files"},
    {"mv",      cmd_mv,      "Move/rename files"},
    {"touch",   cmd_touch,   "Create empty file"},
    {"cat",     cmd_cat,     "Display file content"},
    {"head",    cmd_head,    "First lines of file"},
    {"tail",    cmd_tail,    "Last lines of file"},
    {"grep",    cmd_grep,    "Search text in files"},
    {"wc",      cmd_wc,      "Count lines/words/bytes"},
    {"reboot",  cmd_reboot,  "Restart system"},
    {"halt",    cmd_reboot,  "Restart system"},
    {"poweroff",cmd_reboot,  "Restart system"},
    {"dmesg",   cmd_hexdump, "View kernel messages"},
    {"uname",   cmd_uname,   "System information"},
    {"uptime",  cmd_uptime,  "System uptime"},
    {"free",    cmd_free,    "Memory usage (simulated)"},
    {"df",      cmd_df,      "Disk space usage"},
    {"du",      cmd_du,      "Directory space usage"},
    {"sleep",   cmd_sleep,   "Delay execution"},
    {"ps",      cmd_ls,      "List running processes (simulated)"},
    {"kill",    cmd_echo,    "Terminate process (simulated)"},
    {"date",    cmd_date,    "Show current date and time"},
    {"whoami",  cmd_whoami,  "Show current user"},
    {"nano",    cmd_nano,    "Simple text editor"},
    {"vim",     cmd_vim,     "Simple text editor (alias for nano)"},
    {"hexdump", cmd_hexdump, "Dump kernel memory"},
    {"print",   cmd_echo,    "Print text (alias for echo)"},
    {"mem",     cmd_memory,  "Show actual memory usage"},
    {"memory",  cmd_memory,  "Show actual memory usage"}
};

int cmd_count = sizeof(cmd_table) / sizeof(cmd_entry_t);

// ============================================
// POSIX Command Implementations
// ============================================

void cmd_help(const char* args) {
    (void)args;
    PrintLn("Available commands:", COLOR_YELLOW, COLOR_BLACK);
    for (int i = 0; i < cmd_count; i++) {
        Print("  ", COLOR_WHITE, COLOR_BLACK);
        Print(cmd_table[i].name, COLOR_LIGHT_GREEN, COLOR_BLACK);
        Print(" - ", COLOR_DARK_GREY, COLOR_BLACK);
        PrintLn(cmd_table[i].description, COLOR_WHITE, COLOR_BLACK);
    }
}

void cmd_echo(const char* args) {
    if (args && args[0] != '\0') {
        PrintLn(args, COLOR_WHITE, COLOR_BLACK);
    } else {
        PrintLn("", COLOR_WHITE, COLOR_BLACK);
    }
}

void cmd_clear(const char* args) {
    (void)args;
    ClearScreen();
    PrintLn("Screen cleared. Type 'help' for commands.", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_reboot(const char* args) {
    (void)args;
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

void cmd_hexdump(const char* args) {
    (void)args;
    memory_alloc(4);
    PrintLn("Kernel memory dump (0x100000):", COLOR_LIGHT_GREY, COLOR_BLACK);
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
    memory_free(4);
}

void cmd_ls(const char* args) {
    (void)args;
    PrintLn("Directory: ", COLOR_YELLOW, COLOR_BLACK);
    PrintLn(current_dir, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  README.TXT    (1024 bytes)", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  KERNEL.BIN    (16384 bytes)", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  GRACE.TXT     (512 bytes)", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  FERRITE.ENG   (314159 bytes)", COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    PrintLn("  BIN/          (directory)", COLOR_LIGHT_BLUE, COLOR_BLACK);
    PrintLn("  ETC/          (directory)", COLOR_LIGHT_BLUE, COLOR_BLACK);
}

void cmd_pwd(const char* args) {
    (void)args;
    PrintLn(current_dir, COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_cd(const char* args) {
    if (!args || args[0] == '\0') {
        current_dir[0] = '/';
        current_dir[1] = '\0';
    } else if (args[0] == '/' && args[1] == '\0') {
        current_dir[0] = '/';
        current_dir[1] = '\0';
    } else {
        Print("cd: ", COLOR_LIGHT_RED, COLOR_BLACK);
        Print(args, COLOR_LIGHT_RED, COLOR_BLACK);
        PrintLn(": Directory not implemented yet", COLOR_LIGHT_RED, COLOR_BLACK);
    }
}

void cmd_exit(const char* args) {
    (void)args;
    PrintLn("Goodbye!", COLOR_LIGHT_GREY, COLOR_BLACK);
    while(1) {
        __asm__ volatile("hlt");
    }
}

void cmd_touch(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("touch: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("touch: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": File created (simulated)", COLOR_LIGHT_GREEN, COLOR_BLACK);
    memory_alloc(1);
    memory_free(1);
}

void cmd_cat(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("cat: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    PrintLn("Content of ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(":", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("  [Simulated content of file]", COLOR_WHITE, COLOR_BLACK);
}

void cmd_mkdir(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("mkdir: missing directory operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("mkdir: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": Directory created (simulated)", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_rm(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("rm: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("rm: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": Removed (simulated)", COLOR_LIGHT_RED, COLOR_BLACK);
}

void cmd_rmdir(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("rmdir: missing directory operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("rmdir: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": Directory removed (simulated)", COLOR_LIGHT_RED, COLOR_BLACK);
}

void cmd_cp(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("cp: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("cp: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": Copied (simulated)", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_mv(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("mv: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("mv: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": Moved (simulated)", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_grep(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("grep: missing pattern", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("grep: Searching for '", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("' (simulated)", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_head(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("head: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("head: First 10 lines of ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("  [Simulated output]", COLOR_WHITE, COLOR_BLACK);
}

void cmd_tail(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("tail: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("tail: Last 10 lines of ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("  [Simulated output]", COLOR_WHITE, COLOR_BLACK);
}

void cmd_wc(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("wc: missing file operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("wc: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": 10 50 300 (simulated)", COLOR_LIGHT_CYAN, COLOR_BLACK);
}

void cmd_uname(const char* args) {
    (void)args;
    PrintLn("IncedenaryOS v2026.8", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn("Kernel: IncedenaryOS 1.0.0", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Architecture: i386", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_uptime(const char* args) {
    (void)args;
    PrintLn("Uptime: 0 days, 0 hours, 0 minutes (simulated)", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_free(const char* args) {
    (void)args;
    PrintLn("Memory usage (simulated):", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  Total: 16 MB", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  Used:  4 MB", COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn("  Free:  12 MB", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_memory(const char* args) {
    (void)args;
    PrintLn("Actual Memory Usage:", COLOR_YELLOW, COLOR_BLACK);
    Print("  Total: ", COLOR_WHITE, COLOR_BLACK);
    PrintHex(total_memory_kb);
    PrintLn(" KB", COLOR_WHITE, COLOR_BLACK);
    Print("  Used:  ", COLOR_WHITE, COLOR_BLACK);
    PrintHex(used_memory_kb);
    PrintLn(" KB", COLOR_WHITE, COLOR_BLACK);
    Print("  Free:  ", COLOR_WHITE, COLOR_BLACK);
    PrintHex(total_memory_kb - used_memory_kb);
    PrintLn(" KB", COLOR_WHITE, COLOR_BLACK);
    Print("  Peak:  ", COLOR_WHITE, COLOR_BLACK);
    PrintHex(peak_memory_kb);
    PrintLn(" KB", COLOR_WHITE, COLOR_BLACK);
}

void cmd_df(const char* args) {
    (void)args;
    PrintLn("Disk space usage:", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  /dev/hda: 1.4 MB / 2.8 MB (50%)", COLOR_WHITE, COLOR_BLACK);
}

void cmd_du(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("du: Directory: / (simulated)", COLOR_LIGHT_GREY, COLOR_BLACK);
        PrintLn("  1.2 MB", COLOR_LIGHT_CYAN, COLOR_BLACK);
        return;
    }
    Print("du: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": 64 KB (simulated)", COLOR_LIGHT_CYAN, COLOR_BLACK);
}

void cmd_date(const char* args) {
    (void)args;
    PrintLn("2026-07-26 12:00:00 UTC (simulated)", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_whoami(const char* args) {
    (void)args;
    PrintLn("root", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_sleep(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("sleep: missing time operand", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    int seconds = 1;
    if (args[0] >= '0' && args[0] <= '9') {
        seconds = args[0] - '0';
    }
    Print("Sleeping for ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    for (volatile int i = 0; i < seconds * 10000000; i++);
}

// ============================================
// Text Editor (nano/vim style)
// ============================================

#define EDITOR_BUFFER_SIZE 4096
#define MAX_LINES 100
#define MAX_LINE_LEN 80

static char editor_buffer[EDITOR_BUFFER_SIZE];
static int editor_len = 0;
static int editor_cursor = 0;

void cmd_editor(const char* filename) {
    if (!filename || filename[0] == '\0') {
        PrintLn("Usage: nano <filename>", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    
    ClearScreen();
    PrintLn("=== IncedenaryOS Text Editor ===", COLOR_YELLOW, COLOR_BLACK);
    Print("File: ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(filename, COLOR_LIGHT_CYAN, COLOR_BLACK);
    PrintLn("--- Edit mode. Type text. ESC to save and exit. ---", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    editor_len = 0;
    editor_cursor = 0;
    editor_buffer[0] = '\0';
    
    while (1) {
        ClearScreen();
        PrintLn("=== IncedenaryOS Text Editor ===", COLOR_YELLOW, COLOR_BLACK);
        Print("File: ", COLOR_LIGHT_GREY, COLOR_BLACK);
        PrintLn(filename, COLOR_LIGHT_CYAN, COLOR_BLACK);
        PrintLn("--- ESC to save and exit ---", COLOR_LIGHT_GREY, COLOR_BLACK);
        PrintLn("", COLOR_WHITE, COLOR_BLACK);
        
        int line = 0;
        int pos = 0;
        while (pos < editor_len && line < MAX_LINES) {
            int line_start = pos;
            while (pos < editor_len && editor_buffer[pos] != '\n') pos++;
            char line_buffer[MAX_LINE_LEN + 1];
            int line_len = pos - line_start;
            if (line_len > MAX_LINE_LEN) line_len = MAX_LINE_LEN;
            for (int i = 0; i < line_len; i++) {
                line_buffer[i] = editor_buffer[line_start + i];
            }
            line_buffer[line_len] = '\0';
            
            if (editor_cursor >= line_start && editor_cursor <= pos) {
                for (int i = 0; i < line_len; i++) {
                    PutCharAt(line_buffer[i], i, 4 + line, COLOR_WHITE, COLOR_BLACK);
                }
                int cursor_col = editor_cursor - line_start;
                if (cursor_col >= 0 && cursor_col < VGA_WIDTH) {
                    PutCharAt('_', cursor_col, 4 + line, COLOR_LIGHT_GREEN, COLOR_BLACK);
                }
            } else {
                PrintLn(line_buffer, COLOR_WHITE, COLOR_BLACK);
            }
            line++;
            if (pos < editor_len && editor_buffer[pos] == '\n') pos++;
        }
        
        char c = get_key();
        
        if (c == 0x1B) {  // Escape key
            PrintLn("\nSaving...", COLOR_LIGHT_GREEN, COLOR_BLACK);
            PrintLn("File saved (simulated).", COLOR_LIGHT_GREEN, COLOR_BLACK);
            PrintLn("Press any key to return to shell.", COLOR_LIGHT_GREY, COLOR_BLACK);
            get_key();
            ClearScreen();
            return;
        } else if (c == '\b') {
            if (editor_cursor > 0) {
                for (int i = editor_cursor - 1; i < editor_len; i++) {
                    editor_buffer[i] = editor_buffer[i + 1];
                }
                editor_len--;
                editor_cursor--;
            }
        } else if (c == '\n') {
            if (editor_len < EDITOR_BUFFER_SIZE - 1) {
                for (int i = editor_len; i > editor_cursor; i--) {
                    editor_buffer[i] = editor_buffer[i - 1];
                }
                editor_buffer[editor_cursor] = '\n';
                editor_len++;
                editor_cursor++;
            }
        } else if (c >= 32 && c <= 126) {
            if (editor_len < EDITOR_BUFFER_SIZE - 1) {
                for (int i = editor_len; i > editor_cursor; i--) {
                    editor_buffer[i] = editor_buffer[i - 1];
                }
                editor_buffer[editor_cursor] = c;
                editor_len++;
                editor_cursor++;
            }
        }
    }
}

void cmd_nano(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("Usage: nano <filename>", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    cmd_editor(args);
}

void cmd_vim(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("Usage: vim <filename>", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    cmd_editor(args);
}

// ============================================
// FAT Demo Functions
// ============================================

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
// Shell
// ============================================

void shell(void) {
    char input[BUFFER_SIZE];
    int input_len = 0;
    ClearScreen();
    PrintLn("IncedenaryOS v2026.8 - POSIX Shell", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Type 'help' for commands", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    while (1) {
        Print("root:", COLOR_LIGHT_GREEN, COLOR_BLACK);
        Print(current_dir, COLOR_LIGHT_BLUE, COLOR_BLACK);
        Print("$ ", COLOR_WHITE, COLOR_BLACK);
        
        input_len = 0;
        while (1) {
            char c = get_key();
            
            // Check for scrolling keys (Page Up / Page Down)
            if (c == 0x49) {  // Page Up
                if (scroll_offset < scrollback_count - (VGA_HEIGHT - 2)) {
                    scroll_offset++;
                    in_scroll_mode = 1;
                    scrollback_display();
                }
                continue;
            } else if (c == 0x51) {  // Page Down
                if (scroll_offset > 0) {
                    scroll_offset--;
                    if (scroll_offset == 0) {
                        in_scroll_mode = 0;
                        ClearScreen();
                        PrintLn("IncedenaryOS v2026.8 - POSIX Shell", COLOR_LIGHT_GREY, COLOR_BLACK);
                        PrintLn("Type 'help' for commands", COLOR_LIGHT_GREY, COLOR_BLACK);
                        PrintLn("", COLOR_WHITE, COLOR_BLACK);
                    } else {
                        scrollback_display();
                    }
                } else if (in_scroll_mode) {
                    in_scroll_mode = 0;
                    ClearScreen();
                    PrintLn("IncedenaryOS v2026.8 - POSIX Shell", COLOR_LIGHT_GREY, COLOR_BLACK);
                    PrintLn("Type 'help' for commands", COLOR_LIGHT_GREY, COLOR_BLACK);
                    PrintLn("", COLOR_WHITE, COLOR_BLACK);
                }
                continue;
            } else if (c == 0x1B && in_scroll_mode) {  // ESC to exit scroll
                in_scroll_mode = 0;
                scroll_offset = 0;
                ClearScreen();
                PrintLn("IncedenaryOS v2026.8 - POSIX Shell", COLOR_LIGHT_GREY, COLOR_BLACK);
                PrintLn("Type 'help' for commands", COLOR_LIGHT_GREY, COLOR_BLACK);
                PrintLn("", COLOR_WHITE, COLOR_BLACK);
                continue;
            }
            
            if (in_scroll_mode) {
                // Don't process input while in scroll mode
                continue;
            }
            
            if (c == '\n') {
                Print("\n", COLOR_WHITE, COLOR_BLACK);
                break;
            } else if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    cursor_x--;
                    PutCharAt(' ', cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
                }
            } else if (c >= 32 && c <= 126 && input_len < BUFFER_SIZE - 1) {
                input[input_len++] = c;
                PutCharAt(c, cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
                cursor_x++;
                if (cursor_x >= VGA_WIDTH) {
                    cursor_x = 0;
                    cursor_y++;
                    if (cursor_y >= VGA_HEIGHT - 1) {
                        scroll_screen();
                        cursor_y = VGA_HEIGHT - 2;
                    }
                }
            }
        }
        input[input_len] = '\0';
        
        char* cmd = input;
        char* args = NULL;
        for (int i = 0; input[i] != '\0'; i++) {
            if (input[i] == ' ') {
                input[i] = '\0';
                args = &input[i + 1];
                break;
            }
        }
        
        int found = 0;
        for (int i = 0; i < cmd_count; i++) {
            if (strcmp(cmd, cmd_table[i].name) == 0) {
                cmd_table[i].func(args);
                found = 1;
                break;
            }
        }
        if (!found && cmd[0] != '\0') {
            Print("Unknown command: ", COLOR_LIGHT_RED, COLOR_BLACK);
            PrintLn(cmd, COLOR_LIGHT_RED, COLOR_BLACK);
        }
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
    
    memory_alloc(1024);  // Kernel memory allocation (simulated)
    
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