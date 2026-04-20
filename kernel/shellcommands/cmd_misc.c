#include "shell_defs.h"
#include "cmd_misc.h"

/* ── repeat N comando ─────────────────────────────────────────────────────── */
void cmd_repeat(const char *line) {
    const char *rest = arg1(line);
    if (*rest < '1' || *rest > '9') { vga_println("Uso: repeat N comando"); return; }

    int n = 0;
    while (*rest >= '0' && *rest <= '9') { n = n * 10 + (*rest - '0'); rest++; }
    rest = ltrim(rest);
    if (k_strlen(rest) == 0) { vga_println("Uso: repeat N comando"); return; }
    if (n > 100) { vga_println("Maximo 100 repeticiones."); return; }

    if (starts_with_cmd(rest, "repeat")) {
        vga_println("Error: repeat no puede anidarse.");
        return;
    }

    char saved[INPUT_MAX];
    int saved_len = input_len;
    for (int i = 0; i < INPUT_MAX; i++) saved[i] = input[i];

    int rest_offset = (int)(rest - input);
    const char *rest_in_saved = saved + rest_offset;

    for (int i = 0; i < n; i++) {
        int j = 0;
        while (rest_in_saved[j] && j < INPUT_MAX - 1) { input[j] = rest_in_saved[j]; j++; }
        input[j] = '\0';
        input_len = j;
        dispatch();
    }
    for (int i = 0; i < INPUT_MAX; i++) input[i] = saved[i];
    input_len = saved_len;
}

/* ── help — abre help.tpbi desde /home ───────────────────────────────────── */
void cmd_help(void) {
    int home = fs_find(fs_root(), "home");
    if (home < 0) { vga_println("Error: directorio home no encontrado."); return; }

    int saved_cwd = cwd;
    cwd = home;

    const char *cmd_str = "open help.tpbi";
    int j = 0;
    while (cmd_str[j] && j < INPUT_MAX - 1) { input[j] = cmd_str[j]; j++; }
    input[j] = '\0';
    input_len = j;
    dispatch();

    cwd = saved_cwd;
}
