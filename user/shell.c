#include "syscall.h"

#define NULL ((void*)0)

#define LINE_MAX 256
#define MAX_ARGS 32
#define MAX_ENV 32
#define ENV_VAL_MAX 128
#define HISTORY_SIZE 16

static char env_vars[MAX_ENV][ENV_VAL_MAX];
static int env_count = 0;
static int last_exit_status = 0;
static char history[HISTORY_SIZE][LINE_MAX];
static int history_count = 0;

static void sys_write_str(const char *s) {
    int len = 0;
    while (s[len]) len++;
    sys_write(0, s, len);
}

static int strlen(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

static void strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++));
}

static void strncpy(char *dst, const char *src, int n) {
    for (int i = 0; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[n - 1] = '\0';
}

static const char *env_get(const char *name) {
    int name_len = strlen(name);
    for (int i = 0; i < env_count; i++) {
        int j = 0;
        while (env_vars[i][j] && env_vars[i][j] != '=' && j < name_len) {
            if (env_vars[i][j] != name[j]) break;
            j++;
        }
        if (j == name_len && env_vars[i][j] == '=')
            return env_vars[i] + j + 1;
    }
    if (strcmp(name, "?") == 0) {
        static char s[4];
        int v = last_exit_status;
        s[0] = '0' + (v / 100); v %= 100;
        s[1] = '0' + (v / 10); v %= 10;
        s[2] = '0' + v;
        s[3] = '\0';
        return s;
    }
    return "";
}

static void env_set(const char *name, const char *value) {
    int name_len = strlen(name);
    int val_len = strlen(value);
    if (name_len + 1 + val_len >= ENV_VAL_MAX) return;

    for (int i = 0; i < env_count; i++) {
        int j = 0;
        while (env_vars[i][j] && env_vars[i][j] != '=' && j < name_len) {
            if (env_vars[i][j] != name[j]) break;
            j++;
        }
        if (j == name_len && env_vars[i][j] == '=') {
            int pos = name_len + 1;
            for (int k = 0; k < val_len; k++) env_vars[i][pos++] = value[k];
            env_vars[i][pos] = '\0';
            return;
        }
    }
    if (env_count < MAX_ENV) {
        int pos = 0;
        for (int k = 0; k < name_len; k++) env_vars[env_count][pos++] = name[k];
        env_vars[env_count][pos++] = '=';
        for (int k = 0; k < val_len; k++) env_vars[env_count][pos++] = value[k];
        env_vars[env_count][pos] = '\0';
        env_count++;
    }
}

static void env_list(void) {
    for (int i = 0; i < env_count; i++) {
        sys_write_str(env_vars[i]);
        sys_write_str("\n");
    }
}

static void history_add(const char *line) {
    if (!line[0]) return;
    if (history_count > 0 && strcmp(history[history_count - 1], line) == 0) return;
    if (history_count < HISTORY_SIZE) {
        strcpy(history[history_count++], line);
    } else {
        for (int i = 0; i < HISTORY_SIZE - 1; i++) strcpy(history[i], history[i + 1]);
        strcpy(history[HISTORY_SIZE - 1], line);
    }
}

static void history_list(void) {
    for (int i = 0; i < history_count; i++) {
        char num[8];
        int n = i + 1, d = 0, t = n;
        while (t > 0) { d++; t /= 10; }
        num[d] = '\0';
        t = n;
        for (int j = d - 1; j >= 0; j--) { num[j] = '0' + (t % 10); t /= 10; }
        sys_write_str("  ");
        sys_write_str(num);
        sys_write_str("  ");
        sys_write_str(history[i]);
        sys_write_str("\n");
    }
}

static int expand_vars(const char *src, char *dst, int max) {
    int di = 0;
    for (int si = 0; src[si] && di < max - 1; si++) {
        if (src[si] == '$') {
            si++;
            if (src[si] == '{') {
                si++;
                int ni = 0; char name[64];
                while (src[si] && src[si] != '}' && ni < 63) name[ni++] = src[si++];
                name[ni] = '\0';
                if (src[si] == '}') si++;
                const char *val = env_get(name);
                for (int v = 0; val[v] && di < max - 1; v++) dst[di++] = val[v];
            } else {
                int ni = 0; char name[64];
                while (src[si] && ((src[si] >= 'a' && src[si] <= 'z') ||
                                     (src[si] >= 'A' && src[si] <= 'Z') ||
                                     (src[si] >= '0' && src[si] <= '9') ||
                                      src[si] == '_' || src[si] == '?' || src[si] == '$') && ni < 63)
                    name[ni++] = src[si++];
                name[ni] = '\0';
                if (ni == 0) { dst[di++] = '$'; if (src[si]) dst[di++] = src[si]; }
                else { const char *val = env_get(name); for (int v = 0; val[v] && di < max - 1; v++) dst[di++] = val[v]; si--; }
            }
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
    return di;
}

static int tokenize(char *input, char **argv, int max_args) {
    int argc = 0;
    int i = 0;
    while (input[i] && argc < max_args - 1) {
        while (input[i] == ' ' || input[i] == '\t') i++;
        if (!input[i]) break;
        int start = i;
        int in_sq = 0, in_dq = 0;
        while (input[i] && (in_sq || in_dq || (input[i] != ' ' && input[i] != '\t'))) {
            if (in_sq) { if (input[i] == '\'') { in_sq = 0; i++; continue; } i++; }
            else if (in_dq) { if (input[i] == '"') { in_dq = 0; i++; continue; } i++; }
            else { if (input[i] == '\'') { in_sq = 1; i++; continue; }
                   if (input[i] == '"') { in_dq = 1; i++; continue; }
                   if (input[i] == '\\') { i++; if (input[i]) i++; continue; }
                   i++; }
        }
        int end = i;
        if (argc < max_args - 1) {
            argv[argc] = &input[start];
            if (end < max_args * 4) input[end] = '\0';
            argc++;
        }
    }
    argv[argc] = 0;
    return argc;
}

static void cmd_help(void) {
    sys_write_str("Commands:\n");
    sys_write_str("  help       Show this help\n");
    sys_write_str("  clear      Clear screen\n");
    sys_write_str("  echo [..]  Print args\n");
    sys_write_str("  exit [n]   Exit shell\n");
    sys_write_str("  env        List env vars\n");
    sys_write_str("  export n=v Set env var\n");
    sys_write_str("  unset n    Unset env var\n");
    sys_write_str("  history    Show history\n");
    sys_write_str("  true/false Return 0/1\n");
}

static int execute_builtin(int argc, char **argv) {
    if (argc == 0) return 0;
    if (strcmp(argv[0], "help") == 0) { cmd_help(); return 0; }
    if (strcmp(argv[0], "clear") == 0) { sys_clear(); return 0; }
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) sys_write_str(" ");
            sys_write_str(argv[i]);
        }
        sys_write_str("\n");
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        int status = 0;
        if (argc > 1) {
            for (int i = 0; argv[1][i]; i++) {
                if (argv[1][i] >= '0' && argv[1][i] <= '9')
                    status = status * 10 + (argv[1][i] - '0');
                else break;
            }
        }
        sys_exit(status);
        return 0;
    }
    if (strcmp(argv[0], "env") == 0) { env_list(); return 0; }
    if (strcmp(argv[0], "export") == 0) {
        for (int i = 1; i < argc; i++) {
            char *eq = 0;
            for (int j = 0; argv[i][j]; j++) if (argv[i][j] == '=') { eq = &argv[i][j + 1]; argv[i][j] = '\0'; break; }
            if (eq) env_set(argv[i], eq);
        }
        return 0;
    }
    if (strcmp(argv[0], "unset") == 0) {
        if (argc > 1) {
            for (int i = 0; i < env_count; i++) {
                int j = 0;
                while (env_vars[i][j] && env_vars[i][j] != '=') j++;
                if (env_vars[i][j] == '=' && strcmp(env_vars[i], argv[1]) == 0) {
                    for (int k = i; k < env_count - 1; k++) strcpy(env_vars[k], env_vars[k + 1]);
                    env_count--;
                    break;
                }
            }
        }
        return 0;
    }
    if (strcmp(argv[0], "history") == 0) { history_list(); return 0; }
    if (strcmp(argv[0], "true") == 0) return 0;
    if (strcmp(argv[0], "false") == 0) return 1;
    return -1;
}

static void execute_command(char *cmdline) {
    char expanded[LINE_MAX];
    expand_vars(cmdline, expanded, LINE_MAX);
    char *argv[MAX_ARGS];
    int argc = tokenize(expanded, argv, MAX_ARGS);
    if (argc == 0) return;
    int ret = execute_builtin(argc, argv);
    if (ret == -1) {
        sys_write_str(argv[0]);
        sys_write_str(": command not found\n");
        last_exit_status = 127;
    } else {
        last_exit_status = ret;
    }
}

static void show_prompt(void) {
    const char *ps1 = env_get("PS1");
    if (ps1 && ps1[0]) sys_write_str(ps1);
    else sys_write_str("$ ");
}

static void backspace_n(int n) {
    for (int i = 0; i < n; i++) sys_write_str("\b");
    for (int i = 0; i < n; i++) sys_write_str(" ");
    for (int i = 0; i < n; i++) sys_write_str("\b");
}

static void read_line(char *buf, int max) {
    int pos = 0, len = 0;
    int hist_idx = history_count;

    for (;;) {
        int c = sys_read_raw();

        if (c == 0x1B) continue;

        if (c == 0x11) {
            if (hist_idx > 0) {
                hist_idx--;
                backspace_n(len);
                strcpy(buf, history[hist_idx]);
                len = strlen(buf);
                pos = len;
                sys_write_str(buf);
            }
            continue;
        }

        if (c == 0x12) {
            if (hist_idx < history_count) {
                hist_idx++;
                backspace_n(len);
                if (hist_idx == history_count) {
                    buf[0] = '\0';
                    len = 0;
                    pos = 0;
                } else {
                    strcpy(buf, history[hist_idx]);
                    len = strlen(buf);
                    pos = len;
                    sys_write_str(buf);
                }
            }
            continue;
        }

        if (c == 0x13) {
            if (pos > 0) { pos--; sys_write_str("\b"); }
            continue;
        }

        if (c == 0x14) {
            if (pos < len) {
                sys_write_str(&buf[pos]);
                int moved = len - pos;
                pos = len;
                for (int i = 0; i < moved; i++) sys_write_str("\b");
            }
            continue;
        }

        if (c == 0x0C) {
            sys_clear();
            show_prompt();
            sys_write_str(buf);
            for (int i = len; i > pos; i--) sys_write_str("\b");
            continue;
        }

        if (c == '\n') {
            buf[len] = '\0';
            sys_write_str("\n");
            history_add(buf);
            return;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                for (int i = pos; i < len - 1; i++) buf[i] = buf[i + 1];
                len--;
                sys_write_str("\b");
                sys_write_str(&buf[pos]);
                sys_write_str(" ");
                for (int i = pos; i < len + 1; i++) sys_write_str("\b");
            }
            continue;
        }

        if (len < max - 1) {
            for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos] = c;
            pos++;
            len++;
            sys_write_str(&buf[pos - 1]);
            for (int i = pos; i < len; i++) sys_write_str("\b");
        }
    }
}

void _start(void) {
    char line[LINE_MAX];
    env_set("SHELL", "antigravity-sh");
    env_set("USER", "user");
    env_set("HOME", "/");
    env_set("PS1", "$ ");
    sys_write_str("Antigravity OS Shell\n");
    sys_write_str("Type 'help' for commands.\n\n");
    for (;;) {
        show_prompt();
        read_line(line, sizeof(line));
        if (!line[0]) continue;
        execute_command(line);
    }
}
