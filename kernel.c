#include <stdint.h>
#include <stddef.h>

/* VGA text mode framebuffer at 0xB8000 (80x25) */
static volatile uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA color attributes (bg << 4 | fg) */
#define COLOR_DEFAULT 0x07  /* Light gray on black */
#define COLOR_HEADER  0x1F  /* Bright white on blue */
#define COLOR_ACCENT  0x0B  /* Cyan on black */
#define COLOR_GREEN   0x0A  /* Bright green on black */
#define COLOR_YELLOW  0x0E  /* Yellow on black */
#define COLOR_RED     0x0C  /* Bright red on black */
#define COLOR_WHITE   0x0F  /* Bright white on black */

static size_t cursor_row = 0;
static size_t cursor_col = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void update_hw_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void clear_screen(uint8_t color) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hw_cursor();
}

static void scroll_up(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', COLOR_DEFAULT);
    }
    cursor_row = VGA_HEIGHT - 1;
}

static void putchar_color(char c, uint8_t color) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', color);
        }
    } else {
        VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT) {
        scroll_up();
    }
    update_hw_cursor();
}

static void print_color(const char* str, uint8_t color) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        putchar_color(str[i], color);
    }
}

static void print(const char* str) {
    print_color(str, COLOR_DEFAULT);
}

static void print_at(const char* str, size_t row, size_t col, uint8_t color) {
    for (size_t i = 0; str[i] != '\0' && (col + i) < VGA_WIDTH; i++) {
        VGA_BUFFER[row * VGA_WIDTH + col + i] = vga_entry(str[i], color);
    }
}

static int str_eq(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

/* PS/2 Scancode Set 1 lookup tables */
static const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_shift_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static int shift_pressed = 0;

static char keyboard_getchar(void) {
    while (1) {
        uint8_t status = inb(0x64);
        if ((status & 0x01) == 0) continue;
        if (status & 0x20) {
            (void)inb(0x60);
            continue;
        }
        uint8_t sc = inb(0x60);

        if (sc == 0x2A || sc == 0x36) {
            shift_pressed = 1;
            continue;
        }
        if (sc == 0xAA || sc == 0xB6) {
            shift_pressed = 0;
            continue;
        }
        if (sc & 0x80) continue;

        if (sc < 128) {
            char c = shift_pressed ? scancode_shift_map[sc] : scancode_map[sc];
            if (c != 0) return c;
        }
    }
}

static int keyboard_has_key(void) {
    uint8_t status = inb(0x64);
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(0x60);
        if (!(sc & 0x80) && sc != 0x2A && sc != 0x36) {
            return 1;
        }
    }
    return 0;
}

static void delay_loop(volatile uint32_t count) {
    while (count--) {
        __asm__ volatile ("nop");
    }
}

static void show_banner(void) {
    clear_screen(COLOR_DEFAULT);
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[x] = vga_entry(' ', COLOR_HEADER);
    }
    print_at("  SARP OS v0.2  |  32-bit x86 Protected Mode  |  VGA Text Console", 0, 0, COLOR_HEADER);

    cursor_row = 2;
    cursor_col = 0;
    print_color("[OK] ", COLOR_GREEN); print("Multiboot 32-bit protected mode initialized\n");
    print_color("[OK] ", COLOR_GREEN); print("VGA text buffer mapped at 0xB8000 (80x25)\n");
    print_color("[OK] ", COLOR_GREEN); print("PS/2 keyboard controller ready (port 0x60)\n\n");
    print("Type ");
    print_color("help", COLOR_YELLOW);
    print(" for available commands.\n\n");
    update_hw_cursor();
}

static void cmd_help(void) {
    print_color("Commands:\n", COLOR_ACCENT);
    print_color("  help     ", COLOR_YELLOW); print("- Show available commands\n");
    print_color("  sysinfo  ", COLOR_YELLOW); print("- Display kernel and hardware interface info\n");
    print_color("  palette  ", COLOR_YELLOW); print("- Render 16-color VGA attribute test\n");
    print_color("  matrix   ", COLOR_YELLOW); print("- Run VGA framebuffer rain demo (any key exits)\n");
    print_color("  clear    ", COLOR_YELLOW); print("- Clear console\n");
    print_color("  reboot   ", COLOR_YELLOW); print("- Trigger PS/2 controller CPU reset\n\n");
}

static void cmd_sysinfo(void) {
    print_color("System Information:\n", COLOR_ACCENT);
    print("  Kernel   : Sarp OS v0.2 (freestanding C + NASM)\n");
    print("  Arch     : x86 32-bit Protected Mode (Multiboot)\n");
    print("  Video    : VGA Text Mode 80x25 @ 0xB8000\n");
    print("  Input    : PS/2 Scancode Set 1 (polling @ 0x60/0x64)\n\n");
}

static void cmd_palette(void) {
    print_color("VGA 16-Color Palette:\n  ", COLOR_ACCENT);
    for (uint8_t bg = 0; bg < 16; bg++) {
        uint8_t attr = (uint8_t)((bg << 4) | (bg < 8 ? 0x0F : 0x00));
        putchar_color(' ', attr);
        putchar_color('#', attr);
        putchar_color(' ', attr);
    }
    print("\n\n");
}

static void cmd_matrix(void) {
    clear_screen(0x00);
    uint32_t seed = 0x12345678;
    size_t drops[VGA_WIDTH];
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        seed = seed * 1664525u + 1013904223u;
        drops[x] = seed % VGA_HEIGHT;
    }

    print_at(" VGA FRAMEBUFFER DEMO (press any key to exit) ", 0, 17, 0x20);

    while (!keyboard_has_key()) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t y = drops[x];
            if (y > 0 && y < VGA_HEIGHT) {
                seed = seed * 1664525u + 1013904223u;
                char ch = (char)('!' + (seed % 90));
                VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(ch, COLOR_WHITE);
                if (y > 1) {
                    VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = vga_entry(ch, COLOR_GREEN);
                }
                if (y > 5) {
                    VGA_BUFFER[(y - 5) * VGA_WIDTH + x] = vga_entry(' ', 0x00);
                }
            }
            drops[x]++;
            if (drops[x] >= VGA_HEIGHT + 6) {
                drops[x] = 1;
            }
        }
        delay_loop(6000000);
    }

    show_banner();
}

static void cmd_reboot(void) {
    print_color("Rebooting via PS/2 controller...\n", COLOR_YELLOW);
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kernel_main(void) {
    show_banner();

    char cmd_buf[64];
    size_t cmd_len = 0;

    while (1) {
        print_color("sarp-os> ", COLOR_ACCENT);
        cmd_len = 0;

        while (1) {
            char c = keyboard_getchar();
            if (c == '\n') {
                putchar_color('\n', COLOR_DEFAULT);
                cmd_buf[cmd_len] = '\0';
                break;
            } else if (c == '\b') {
                if (cmd_len > 0) {
                    cmd_len--;
                    putchar_color('\b', COLOR_DEFAULT);
                }
            } else if (cmd_len < sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_len++] = c;
                putchar_color(c, COLOR_WHITE);
            }
        }

        if (cmd_len == 0) {
            continue;
        } else if (str_eq(cmd_buf, "help")) {
            cmd_help();
        } else if (str_eq(cmd_buf, "sysinfo")) {
            cmd_sysinfo();
        } else if (str_eq(cmd_buf, "palette")) {
            cmd_palette();
        } else if (str_eq(cmd_buf, "matrix")) {
            cmd_matrix();
        } else if (str_eq(cmd_buf, "clear")) {
            show_banner();
        } else if (str_eq(cmd_buf, "reboot")) {
            cmd_reboot();
        } else {
            print_color("Unknown command: ", COLOR_RED);
            print_color(cmd_buf, COLOR_WHITE);
            print(" (type 'help')\n\n");
        }
    }
}
