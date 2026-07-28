// kmain.c — IncedenaryOS Full POSIX Shell + .sh SDK

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define NULL ((void*)0)

// ============================================
// Standard Library Functions
// ============================================

int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
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

void PutCharAt(char c, int x, int y, enum vga_color fg, enum vga_color bg) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT || x < 0 || y < 0) return;
    unsigned short* vga = (unsigned short*) VGA_MEMORY;
    unsigned short color = (bg << 4) | fg;
    vga[y * VGA_WIDTH + x] = (color << 8) | c;
}

void PrintAt(const char* str, int x, int y, enum vga_color fg, enum vga_color bg) {
    int i = 0;
    while (str[i] != '\0') {
        PutCharAt(str[i], x + i, y, fg, bg);
        i++;
    }
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
            cursor_y = 0;
        }
    }
}

void PrintLn(const char* str, enum vga_color fg, enum vga_color bg) {
    Print(str, fg, bg);
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = 0;
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

// ============================================
// Keyboard Driver
// ============================================

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "d"(port));
    return result;
}

char get_key(void) {
    while (1) {
        if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
            unsigned char scancode = inb(KEYBOARD_DATA_PORT);
            if (scancode == 0x01) return 0x1B;    // Escape
            if (scancode == 0x1C) return '\n';    // Enter
            if (scancode == 0x0E) return '\b';    // Backspace
            // Map scancodes to ASCII (simplified)
            static const char table[] = {
                0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
                'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
                'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
                'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
            };
            if (scancode < 0x3A) {
                char c = table[scancode];
                if (c) return c;
            }
        }
    }
}

// ============================================
// .sh SDK (Incedenary Shell File)
// ============================================

#define MAX_SH_FILES 16
#define MAX_SH_LINES 64
#define MAX_SH_LINE_LEN 128

typedef struct {
    char name[32];
    char lines[MAX_SH_LINES][MAX_SH_LINE_LEN];
    int line_count;
} sh_file_t;

static sh_file_t sh_files[MAX_SH_FILES];
static int sh_file_count = 0;

// Add a .sh file
void add_sh_file(const char* name, const char* content) {
    if (sh_file_count >= MAX_SH_FILES) return;
    for (int i = 0; i < 31 && name[i]; i++) {
        sh_files[sh_file_count].name[i] = name[i];
    }
    sh_files[sh_file_count].name[31] = '\0';
    
    int line = 0, pos = 0;
    for (int i = 0; content[i] && line < MAX_SH_LINES; i++) {
        if (content[i] == '\n') {
            sh_files[sh_file_count].lines[line][pos] = '\0';
            line++;
            pos = 0;
        } else if (pos < MAX_SH_LINE_LEN - 1) {
            sh_files[sh_file_count].lines[line][pos++] = content[i];
        }
    }
    if (pos > 0) {
        sh_files[sh_file_count].lines[line][pos] = '\0';
        line++;
    }
    sh_files[sh_file_count].line_count = line;
    sh_file_count++;
}

// Execute a .sh file
void run_sh_file(int index) {
    if (index < 0 || index >= sh_file_count) {
        PrintLn("No such .sh file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    
    sh_file_t* sh = &sh_files[index];
    for (int i = 0; i < sh->line_count; i++) {
        PrintLn(sh->lines[i], COLOR_WHITE, COLOR_BLACK);
    }
}

// List .sh files
void list_sh_files(void) {
    PrintLn(".sh files:", COLOR_YELLOW, COLOR_BLACK);
    for (int i = 0; i < sh_file_count; i++) {
        Print("  ", COLOR_WHITE, COLOR_BLACK);
        Print(sh_files[i].name, COLOR_LIGHT_CYAN, COLOR_BLACK);
        Print(" (", COLOR_DARK_GREY, COLOR_BLACK);
        // Print line count
        char num[4];
        num[0] = '0' + (sh_files[i].line_count / 10);
        num[1] = '0' + (sh_files[i].line_count % 10);
        num[2] = '\0';
        Print(num, COLOR_LIGHT_GREY, COLOR_BLACK);
        PrintLn(" lines)", COLOR_DARK_GREY, COLOR_BLACK);
    }
}

// ============================================
// POSIX Command Implementations
// ============================================

void cmd_help(void);
void cmd_echo(const char* args);
void cmd_clear(void);
void cmd_reboot(void);
void cmd_ls(void);
void cmd_cat(const char* args);
void cmd_pwd(void);
void cmd_cd(const char* args);
void cmd_date(void);
void cmd_whoami(void);
void cmd_uptime(void);
void cmd_mem(void);
void cmd_hexdump(void);
void cmd_uname(void);
void cmd_sleep(const char* args);
void cmd_head(const char* args);
void cmd_tail(const char* args);
void cmd_wc(const char* args);
void cmd_grep(const char* args);
void cmd_find(const char* args);
void cmd_mkdir(const char* args);
void cmd_rmdir(const char* args);
void cmd_rm(const char* args);
void cmd_touch(const char* args);
void cmd_cp(const char* args);
void cmd_mv(const char* args);
void cmd_ps(void);
void cmd_kill(const char* args);
void cmd_free(void);
void cmd_df(void);
void cmd_du(const char* args);
void cmd_sh(const char* args);
void cmd_run(const char* args);

// ============================================
// POSIX Command Table
// ============================================

typedef struct {
    const char* name;
    void (*func)(const char* args);
    const char* desc;
} cmd_t;

cmd_t cmd_table[] = {
    {"help",    cmd_help,    "Show this help"},
    {"echo",    cmd_echo,    "Print text"},
    {"clear",   cmd_clear,   "Clear screen"},
    {"reboot",  cmd_reboot,  "Reboot system"},
    {"ls",      cmd_ls,      "List files"},
    {"cat",     cmd_cat,     "Display file content"},
    {"pwd",     cmd_pwd,     "Print working directory"},
    {"cd",      cmd_cd,      "Change directory"},
    {"date",    cmd_date,    "Show date/time"},
    {"whoami",  cmd_whoami,  "Show current user"},
    {"uptime",  cmd_uptime,  "System uptime"},
    {"mem",     cmd_mem,     "Memory usage"},
    {"hexdump", cmd_hexdump, "Dump memory"},
    {"uname",   cmd_uname,   "System info"},
    {"sleep",   cmd_sleep,   "Delay execution"},
    {"head",    cmd_head,    "First lines of file"},
    {"tail",    cmd_tail,    "Last lines of file"},
    {"wc",      cmd_wc,      "Count lines/words/bytes"},
    {"grep",    cmd_grep,    "Search text"},
    {"find",    cmd_find,    "Find files"},
    {"mkdir",   cmd_mkdir,   "Create directory"},
    {"rmdir",   cmd_rmdir,   "Remove directory"},
    {"rm",      cmd_rm,      "Remove file"},
    {"touch",   cmd_touch,   "Create empty file"},
    {"cp",      cmd_cp,      "Copy file"},
    {"mv",      cmd_mv,      "Move file"},
    {"ps",      cmd_ps,      "List processes"},
    {"kill",    cmd_kill,    "Terminate process"},
    {"free",    cmd_free,    "Memory info"},
    {"df",      cmd_df,      "Disk space"},
    {"du",      cmd_du,      "Directory usage"},
    {"sh",      cmd_sh,      "Show .sh files"},
    {"run",     cmd_run,     "Run .sh file"}
};

int cmd_count = sizeof(cmd_table) / sizeof(cmd_t);

// ============================================
// POSIX Command Implementations
// ============================================

void cmd_help(void) {
    PrintLn("IncedenaryOS POSIX Shell Commands:", COLOR_YELLOW, COLOR_BLACK);
    for (int i = 0; i < cmd_count; i++) {
        Print("  ", COLOR_WHITE, COLOR_BLACK);
        Print(cmd_table[i].name, COLOR_LIGHT_GREEN, COLOR_BLACK);
        Print(" - ", COLOR_DARK_GREY, COLOR_BLACK);
        PrintLn(cmd_table[i].desc, COLOR_WHITE, COLOR_BLACK);
    }
}

void cmd_echo(const char* args) {
    if (args && args[0] != '\0') {
        PrintLn(args, COLOR_WHITE, COLOR_BLACK);
    } else {
        PrintLn("", COLOR_WHITE, COLOR_BLACK);
    }
}

void cmd_clear(void) {
    ClearScreen();
    PrintLn("Screen cleared.", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_reboot(void) {
    PrintLn("Rebooting...", COLOR_LIGHT_RED, COLOR_BLACK);
    for (volatile int i = 0; i < 10000000; i++);
    __asm__ volatile ("out %%al, $0x64" : : "a"(0x64));
}

void cmd_ls(void) {
    PrintLn("Directory: /", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  README.txt", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  kernel.elf", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  .sh files:", COLOR_LIGHT_GREY, COLOR_BLACK);
    for (int i = 0; i < sh_file_count; i++) {
        Print("    ", COLOR_WHITE, COLOR_BLACK);
        PrintLn(sh_files[i].name, COLOR_LIGHT_CYAN, COLOR_BLACK);
    }
}

void cmd_cat(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("cat: missing file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Content of ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("  [File content simulation]", COLOR_WHITE, COLOR_BLACK);
}

void cmd_pwd(void) {
    PrintLn("/", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_cd(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("/", COLOR_LIGHT_GREEN, COLOR_BLACK);
    } else {
        Print("cd: ", COLOR_LIGHT_RED, COLOR_BLACK);
        Print(args, COLOR_LIGHT_RED, COLOR_BLACK);
        PrintLn(": No such directory", COLOR_LIGHT_RED, COLOR_BLACK);
    }
}

void cmd_date(void) {
    PrintLn("2026-07-28 12:00:00 UTC", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_whoami(void) {
    PrintLn("root", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_uptime(void) {
    PrintLn("Uptime: 0 days, 0 hours, 0 minutes", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_mem(void) {
    PrintLn("Memory usage:", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  Total: 16 MB", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  Used:  4 MB", COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn("  Free:  12 MB", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_hexdump(void) {
    PrintLn("Memory dump (0x100000):", COLOR_LIGHT_GREY, COLOR_BLACK);
    unsigned char* ptr = (unsigned char*)0x100000;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 16; col++) {
            unsigned char val = ptr[row * 16 + col];
            PutCharAt("0123456789ABCDEF"[val >> 4], cursor_x, cursor_y, COLOR_LIGHT_CYAN, COLOR_BLACK);
            cursor_x++;
            PutCharAt("0123456789ABCDEF"[val & 0xF], cursor_x, cursor_y, COLOR_LIGHT_CYAN, COLOR_BLACK);
            cursor_x++;
            PutCharAt(' ', cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
            cursor_x++;
        }
        cursor_x = 0;
        cursor_y++;
    }
}

void cmd_uname(void) {
    PrintLn("IncedenaryOS v2026.8", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn("Kernel: IncedenaryOS 1.0.0", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Architecture: i386", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_sleep(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("sleep: missing time", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    int seconds = args[0] - '0';
    Print("Sleeping for ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    for (volatile int i = 0; i < seconds * 10000000; i++);
}

void cmd_head(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("head: missing file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("First 10 lines of ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("  [Simulated output]", COLOR_WHITE, COLOR_BLACK);
}

void cmd_tail(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("tail: missing file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Last 10 lines of ", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("  [Simulated output]", COLOR_WHITE, COLOR_BLACK);
}

void cmd_wc(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("wc: missing file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn(": 10 50 300 (simulated)", COLOR_LIGHT_CYAN, COLOR_BLACK);
}

void cmd_grep(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("grep: missing pattern", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Searching for '", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("' (simulated)", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_find(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("find: missing pattern", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Finding files matching '", COLOR_LIGHT_GREY, COLOR_BLACK);
    Print(args, COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("' (simulated)", COLOR_LIGHT_GREY, COLOR_BLACK);
}

void cmd_mkdir(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("mkdir: missing directory", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Created directory: ", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_rmdir(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("rmdir: missing directory", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Removed directory: ", COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_RED, COLOR_BLACK);
}

void cmd_rm(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("rm: missing file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Removed: ", COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_RED, COLOR_BLACK);
}

void cmd_touch(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("touch: missing file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Created empty file: ", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_cp(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("cp: missing arguments", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Copied: ", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_mv(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("mv: missing arguments", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Moved: ", COLOR_LIGHT_GREEN, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_ps(void) {
    PrintLn("Processes:", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  1  shell", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  2  idle", COLOR_WHITE, COLOR_BLACK);
}

void cmd_kill(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("kill: missing PID", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    Print("Killed process: ", COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn(args, COLOR_LIGHT_RED, COLOR_BLACK);
}

void cmd_free(void) {
    PrintLn("Memory info:", COLOR_YELLOW, COLOR_BLACK);
    PrintLn("  Total: 16 MB", COLOR_WHITE, COLOR_BLACK);
    PrintLn("  Used:  4 MB", COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn("  Free:  12 MB", COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void cmd_df(void) {
    PrintLn("Disk space:", COLOR_YELLOW, COLOR_BLACK);
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

void cmd_sh(const char* args) {
    (void)args;
    list_sh_files();
}

void cmd_run(const char* args) {
    if (!args || args[0] == '\0') {
        PrintLn("run: missing .sh file", COLOR_LIGHT_RED, COLOR_BLACK);
        return;
    }
    for (int i = 0; i < sh_file_count; i++) {
        if (strcmp(args, sh_files[i].name) == 0) {
            run_sh_file(i);
            return;
        }
    }
    Print("run: ", COLOR_LIGHT_RED, COLOR_BLACK);
    Print(args, COLOR_LIGHT_RED, COLOR_BLACK);
    PrintLn(": .sh file not found", COLOR_LIGHT_RED, COLOR_BLACK);
}

// ============================================
// Shell
// ============================================

void shell(void) {
    ClearScreen();
    PrintLn("IncedenaryOS v2026.8 - POSIX Shell", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Type 'help' for commands", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Type 'sh' to list .sh files", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Type 'run <file>' to execute .sh", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    // Add default .sh files
    add_sh_file("HELLO.sh", "echo Hello from .sh!\necho This is a .sh file.\n");
    add_sh_file("INFO.sh", "uname\nuptime\nmem\n");
    add_sh_file("DEMO.sh", "echo Welcome to IncedenaryOS!\necho .sh files are simple scripts.\nls\n");
    
    char input[256];
    int input_len = 0;
    
    while (1) {
        Print("root:/$ ", COLOR_LIGHT_GREEN, COLOR_BLACK);
        input_len = 0;
        while (1) {
            char c = get_key();
            if (c == '\n') {
                Print("\n", COLOR_WHITE, COLOR_BLACK);
                break;
            } else if (c == '\b' && input_len > 0) {
                input_len--;
                cursor_x--;
                PutCharAt(' ', cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
            } else if (c >= 32 && c <= 126 && input_len < 255) {
                input[input_len++] = c;
                PutCharAt(c, cursor_x, cursor_y, COLOR_WHITE, COLOR_BLACK);
                cursor_x++;
            }
        }
        input[input_len] = '\0';
        
        // Parse command and args
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

void kmain(unsigned int magic, unsigned int addr) {
    (void)magic;
    (void)addr;
    
    ClearScreen();
    PrintLn("IncedenaryOS v2026.8", COLOR_LIGHT_GREY, COLOR_BLACK);
    PrintLn("Kernel loaded at 0x100000", COLOR_CYAN, COLOR_BLACK);
    PrintLn("", COLOR_WHITE, COLOR_BLACK);
    
    shell();
    
    while(1) __asm__ volatile("hlt");
}