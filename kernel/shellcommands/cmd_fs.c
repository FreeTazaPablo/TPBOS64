#include "shell_defs.h"
#include "fs_persist.h"

/* ── copy origen destino ─────────────────────────────────────────────────── */
void cmd_copy(const char *line) {
    /* "copy ORIGEN DESTINO" — copia un archivo con nuevo nombre en mismo dir
       o "copy ORIGEN DIRECTORIO/" para copiarlo dentro de un dir */
    const char *rest = arg1(line);

    /* Buscar el espacio que separa origen de destino */
    int i = 0;
    while (rest[i] && rest[i] != ' ') i++;
    if (i == 0 || rest[i] == '\0') { vga_println("Uso: copy origen destino"); return; }

    char src_name[FS_MAX_NAME];
    int slen = i < FS_MAX_NAME-1 ? i : FS_MAX_NAME-1;
    for (int j = 0; j < slen; j++) src_name[j] = rest[j];
    src_name[slen] = '\0';

    const char *dst_name = ltrim(rest + i + 1);
    if (k_strlen(dst_name) == 0) { vga_println("Uso: copy origen destino"); return; }

    /* Buscar origen */
    int src_idx = resolve_path(src_name);
    if (src_idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(src_name); return;
    }
    FSNode *src = fs_get(src_idx);
    if (src->type != NODE_FILE) { vga_println("Solo se pueden copiar archivos."); return; }

    /* Determinar directorio destino y nombre del nuevo archivo */
    int dst_parent = cwd;
    const char *new_name = dst_name;

    /* Si el destino es un directorio existente, copiar ahí con el mismo nombre */
    int maybe_dir = resolve_path(dst_name);
    if (maybe_dir >= 0 && fs_get(maybe_dir)->type == NODE_DIR) {
        dst_parent = maybe_dir;
        new_name = src_name;
    }

    /* Crear el nuevo archivo */
    int new_idx = fs_create(dst_parent, new_name, NODE_FILE);
    if (new_idx < 0) {
        vga_print_color("Error: ya existe o no hay espacio.\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    /* Copiar contenido */
    const char *content = fs_read(src_idx);
    if (content) fs_write(new_idx, content);

    fs_save_to_disk();
    vga_print("'"); vga_print(src_name);
    vga_print("' copiado a '"); vga_print(new_name); vga_println("'.");
}


/* ── new [file|directory] nombre ─────────────────────────────────────────── */
void cmd_new(const char *line) {
    const char *rest = arg1(line);                 /* "file nombre" o "directory nombre" */
    NodeType type;
    const char *name;

    if (k_strncmp(rest, "file ", 5) == 0) {
        type = NODE_FILE;
        name = ltrim(rest + 5);
    } else if (k_strncmp(rest, "directory ", 10) == 0) {
        type = NODE_DIR;
        name = ltrim(rest + 10);
    } else if (k_strncmp(rest, "dir ", 4) == 0) {
        type = NODE_DIR;
        name = ltrim(rest + 4);
    } else {
        vga_println("Uso: new [file|dir|directory] nombre");
        return;
    }

    if (k_strlen(name) == 0) { vga_println("Nombre vacio."); return; }
    if (k_strlen(name) >= FS_MAX_NAME) { vga_println("Nombre demasiado largo."); return; }

    int idx = fs_create(cwd, name, type);
    if (idx < 0) {
        vga_print_color("Error: ya existe o no hay espacio.\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        fs_save_to_disk();
        vga_print(type == NODE_FILE ? "Archivo" : "Directorio");
        vga_print(" '"); vga_print(name); vga_println("' creado.");
    }
}


/* ── remove nombre ───────────────────────────────────────────────────────── */
void cmd_remove(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) { vga_println("Uso: remove nombre"); return; }
    int idx = resolve_path(name);
    if (idx < 0 || idx == fs_root()) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(name);
        return;
    }
    FSNode *n = fs_get(idx);
    if (fs_remove(idx) == 0) {
        fs_save_to_disk();
        vga_print(n->type == NODE_FILE ? "Archivo" : "Directorio");
        vga_print(" '"); vga_print(name); vga_println("' eliminado.");
    }
}


/* ── move archivo to directorio ──────────────────────────────────────────── */
void cmd_move(const char *line) {
    /* "move NOMBRE to DESTINO" */
    const char *rest = arg1(line);
    /* Buscar " to " */
    char src_name[FS_MAX_NAME];
    int i = 0;
    while (rest[i] && !(rest[i]==' ' && rest[i+1]=='t' && rest[i+2]=='o' && rest[i+3]==' '))
        i++;
    if (i == 0 || rest[i] == '\0') { vga_println("Uso: move archivo to directorio"); return; }
    int slen = i < FS_MAX_NAME-1 ? i : FS_MAX_NAME-1;
    for (int j = 0; j < slen; j++) src_name[j] = rest[j];
    src_name[slen] = '\0';
    const char *dst_name = ltrim(rest + i + 4);

    int src_idx = resolve_path(src_name);
    int dst_idx = resolve_path(dst_name);
    if (src_idx < 0) { vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK); vga_println(src_name); return; }
    if (dst_idx < 0) { vga_print_color("Destino no encontrado: ", VGA_LIGHT_RED, VGA_BLACK); vga_println(dst_name); return; }
    if (fs_get(dst_idx)->type != NODE_DIR) { vga_println("El destino debe ser un directorio."); return; }

    if (fs_move(src_idx, dst_idx) == 0) {
        fs_save_to_disk();
        vga_print("'"); vga_print(src_name); vga_print("' movido a '"); vga_print(dst_name); vga_println("'.");
    }
}


/* ── rename [file|directory] to nuevo_nombre ──────────────────────────────── */
void cmd_rename(const char *line) {
    /* "rename NOMBRE to NUEVO" */
    const char *rest = arg1(line);
    char old_name[FS_MAX_NAME];
    int i = 0;
    while (rest[i] && !(rest[i]==' ' && rest[i+1]=='t' && rest[i+2]=='o' && rest[i+3]==' '))
        i++;
    if (i == 0 || rest[i] == '\0') { vga_println("Uso: rename nombre to nuevo_nombre"); return; }
    int slen = i < FS_MAX_NAME-1 ? i : FS_MAX_NAME-1;
    for (int j = 0; j < slen; j++) old_name[j] = rest[j];
    old_name[slen] = '\0';
    const char *new_name = ltrim(rest + i + 4);

    int idx = resolve_path(old_name);
    if (idx < 0) { vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK); vga_println(old_name); return; }
    if (k_strlen(new_name) == 0 || k_strlen(new_name) >= FS_MAX_NAME) { vga_println("Nombre invalido."); return; }

    if (fs_rename(idx, new_name) == 0) {
        fs_save_to_disk();
        vga_print("'"); vga_print(old_name); vga_print("' renombrado a '"); vga_print(new_name); vga_println("'.");
    }
}


/* ── go directorio ───────────────────────────────────────────────────────── */
void cmd_go(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) { vga_println("Uso: go directorio"); return; }
    /* go # = ir a la raíz directamente */
    if (k_strcmp(name, "#") == 0) { cwd = fs_root(); return; }
    int idx = resolve_path(name);
    if (idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(name); return;
    }
    FSNode *n = fs_get(idx);
    if (!n || n->type != NODE_DIR) { vga_println("No es un directorio."); return; }
    cwd = idx;
}


/* ── list ─────────────────────────────────────────────────────────────────── */
void cmd_list(void) {
    int iter = 0, found = 0;
    int child;
    while ((child = fs_next_child(cwd, &iter)) != -1) {
        FSNode *n = fs_get(child);
        if (n->type == NODE_DIR) {
            vga_print_color("[DIR]  ", VGA_LIGHT_CYAN,  VGA_BLACK);
        } else {
            vga_print_color("[FILE] ", VGA_LIGHT_GREEN, VGA_BLACK);
        }
        vga_println(n->name);
        found = 1;
    }
    if (!found) vga_println("  (vacio)");
}


/* ── open archivo — editor con navegación ────────────────────────────────────
   Ctrl+S = Guardar   Ctrl+Q = Salir sin guardar
   Flechas = mover cursor   Home/End = inicio/fin de línea
   ────────────────────────────────────────────────────────────────────────── */
void cmd_open(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) { vga_println("Uso: open archivo"); return; }

    int idx = resolve_path(name);
    if (idx < 0) {
        idx = fs_create(cwd, name, NODE_FILE);
        if (idx < 0) { vga_println("No se pudo crear el archivo."); return; }
    }
    FSNode *n = fs_get(idx);
    if (n->type != NODE_FILE) { vga_println("No es un archivo."); return; }

    /* Buffer de edición */
    static char buf[FS_MAX_CONTENT];
    int blen = 0;
    k_memset(buf, 0, FS_MAX_CONTENT);
    const char *existing = fs_read(idx);
    if (existing)
        while (existing[blen] && blen < FS_MAX_CONTENT - 1)
            { buf[blen] = existing[blen]; blen++; }
    buf[blen] = '\0';

    int cur = blen;   /* cursor: posición en buf */

    /* ── Renderizado completo del editor ──────────────────────────────────── */
    #define ED_ROWS 23   /* filas disponibles para texto (25 - 2 de status) */

    /* top_line: índice de la primera línea lógica visible en pantalla (0-based) */
    int top_line = 0;

    /* Cuenta el número de línea lógica (0-based) en que se encuentra buf[pos] */
    /* Devuelve también el índice de inicio de esa línea en buf via *line_start_out */
    /* Usamos macros/bloques inline para evitar funciones anidadas en C89 */

    /* ── Loop de edición ──────────────────────────────────────────────────── */
    while (1) {
        /* ── Calcular línea actual del cursor ── */
        int cur_line = 0;
        for (int i = 0; i < cur; i++) if (buf[i] == '\n') cur_line++;

        /* ── Ajustar viewport ── */
        /* Si el cursor quedó por encima del viewport, subir */
        if (cur_line < top_line)
            top_line = cur_line;
        /* Si el cursor quedó por debajo del viewport, bajar */
        if (cur_line >= top_line + ED_ROWS)
            top_line = cur_line - ED_ROWS + 1;

        /* ── Redibujar ── */
        vga_clear();
        vga_print_color("[ TP-BOS Editor ] ", VGA_BLACK, VGA_LIGHT_GRAY);
        vga_print_color(name,                 VGA_BLACK, VGA_LIGHT_GRAY);
        vga_print_color("  Ctrl+S=Guardar  Ctrl+Q=Salir  Ctrl+F=Buscar", VGA_BLACK, VGA_DARK_GRAY);
        vga_putchar('\n');

        /* Encontrar el índice en buf donde comienza top_line */
        int view_start = 0;
        {
            int ln = 0;
            while (ln < top_line && view_start < blen) {
                if (buf[view_start] == '\n') ln++;
                view_start++;
            }
        }

        /* Imprimir desde view_start, solo ED_ROWS lineas.
           Fase 1: imprimir hasta 'cur' — el cursor VGA hardware queda aqui.
           Fase 2: imprimir el resto de las lineas visibles (sin mover el cursor
                   hacia atras, porque vga_putchar no tiene esa primitiva; en su
                   lugar dejamos el cursor sobre cur y no imprimimos mas). */
        {
            int lines_printed = 0;
            int i = view_start;

            /* Fase 1: imprimir hasta cur */
            while (i < cur && lines_printed < ED_ROWS) {
                if (buf[i] == '\n') lines_printed++;
                vga_putchar(buf[i]);
                i++;
            }
            /* Aqui el cursor hardware VGA esta sobre la posicion de cur.
               Fase 2: imprimir el resto para que el texto sea visible,
               pero inmediatamente mover el cursor de vuelta con backspaces. */
            {
                int rest_start = i;
                int extra_bs = 0;   /* cuantos backspaces necesitamos */
                while (i < blen && lines_printed < ED_ROWS) {
                    if (buf[i] == '\n') {
                        lines_printed++;
                        vga_putchar('\n');
                        /* Un '\n' mueve el cursor a la siguiente linea col 0;
                           no podemos retroceder con \b a traves de saltos de linea
                           en un VGA simple. Paramos aqui — el cursor queda en cur. */
                        i++;
                        break;
                    } else {
                        vga_putchar(buf[i]);
                        extra_bs++;
                    }
                    i++;
                }
                /* Retroceder los chars impresos en la misma linea que cur */
                for (int b = 0; b < extra_bs; b++) vga_putchar('\b');
                (void)rest_start;
            }
        }

        unsigned char c = kb_getchar();

        if (c == 17) {   /* Ctrl+Q */
            vga_clear();
            return;
        }
        if (c == 19) {   /* Ctrl+S */
            buf[blen] = '\0';
            fs_write(idx, buf);
            fs_save_to_disk();
            vga_clear();
            vga_print_color("[Guardado] ", VGA_YELLOW, VGA_BLACK);
            vga_println(name);
            return;
        }
        if (c == 6) {   /* Ctrl+F — buscar texto */
            /* ── Modo búsqueda ──────────────────────────────────────────────
               Muestra una barra en la última fila, el usuario escribe un
               patrón y presiona Enter. Luego navega con n/N o Esc para salir.
               ────────────────────────────────────────────────────────────── */
            #define SEARCH_MAX 64
            static char pat[SEARCH_MAX];
            int plen = 0;
            k_memset(pat, 0, SEARCH_MAX);

            /* Dibujar barra de búsqueda */
            vga_clear();
            vga_print_color("[ TP-BOS Editor ] ", VGA_BLACK, VGA_LIGHT_GRAY);
            vga_print_color(name,                 VGA_BLACK, VGA_LIGHT_GRAY);
            vga_print_color("  Ctrl+S=Guardar  Ctrl+Q=Salir  Ctrl+F=Buscar", VGA_BLACK, VGA_DARK_GRAY);
            vga_putchar('\n');
            vga_print_color("Buscar: ", VGA_YELLOW, VGA_BLACK);

            /* Leer patrón */
            while (1) {
                unsigned char pc = kb_getchar();
                if (pc == '\n' || pc == 27 /* Esc */ || pc == 6 /* Ctrl+F */) break;
                if (pc == '\b') {
                    if (plen > 0) { plen--; pat[plen] = '\0'; vga_putchar('\b'); vga_putchar(' '); vga_putchar('\b'); }
                    continue;
                }
                if (pc < 32 || pc >= KEY_UP) continue;
                if (plen < SEARCH_MAX - 1) { pat[plen++] = (char)pc; pat[plen] = '\0'; vga_putchar(pc); }
            }

            if (plen == 0) { continue; }   /* patrón vacío → volver al editor */

            /* Buscar primera ocurrencia desde cur+1 (o desde 0 si no hay) */
            /* Guardamos todos los matches en un arreglo de posiciones       */
            #define MAX_MATCHES 64
            static int matches[MAX_MATCHES];
            int match_count = 0;

            for (int si = 0; si <= blen - plen && match_count < MAX_MATCHES; si++) {
                if (k_strncmp(buf + si, pat, plen) == 0) {
                    matches[match_count++] = si;
                }
            }

            if (match_count == 0) {
                /* Sin resultados: mostrar mensaje y volver */
                vga_clear();
                vga_print_color("[ TP-BOS Editor ] ", VGA_BLACK, VGA_LIGHT_GRAY);
                vga_print_color(name,                 VGA_BLACK, VGA_LIGHT_GRAY);
                vga_print_color("  Ctrl+S=Guardar  Ctrl+Q=Salir  Ctrl+F=Buscar", VGA_BLACK, VGA_DARK_GRAY);
                vga_putchar('\n');
                vga_print_color("No encontrado: '", VGA_LIGHT_RED, VGA_BLACK);
                vga_print_color(pat, VGA_LIGHT_RED, VGA_BLACK);
                vga_print_color("'  Presiona cualquier tecla...", VGA_LIGHT_RED, VGA_BLACK);
                kb_getchar();
                continue;
            }

            /* Encontrar el match más cercano al cursor actual (hacia adelante) */
            int match_idx = 0;
            for (int mi = 0; mi < match_count; mi++) {
                if (matches[mi] >= cur) { match_idx = mi; goto found_match; }
            }
            match_idx = 0;   /* wrap: volver al primero */
            found_match:;

            /* Navegar entre matches con n (siguiente) / N (anterior) / Esc (salir) */
            while (1) {
                /* Posicionar cursor en el match actual */
                cur = matches[match_idx];

                /* Recalcular cur_line y top_line para este match */
                int m_line = 0;
                for (int mi = 0; mi < cur; mi++) if (buf[mi] == '\n') m_line++;
                if (m_line < top_line) top_line = m_line;
                if (m_line >= top_line + ED_ROWS) top_line = m_line - ED_ROWS + 1;

                /* Redibujar editor con el match resaltado */
                vga_clear();
                vga_print_color("[ TP-BOS Editor ] ", VGA_BLACK, VGA_LIGHT_GRAY);
                vga_print_color(name,                 VGA_BLACK, VGA_LIGHT_GRAY);
                vga_print_color("  n=Siguiente  N=Anterior  Esc=Salir busqueda", VGA_BLACK, VGA_DARK_GRAY);
                vga_putchar('\n');

                /* Imprimir contenido con el match resaltado */
                {
                    int vs = 0;
                    { int ln = 0; while (ln < top_line && vs < blen) { if (buf[vs] == '\n') ln++; vs++; } }
                    int lp = 0, i = vs;
                    char hchar[2]; hchar[1] = '\0';   /* buffer de 1 char para resaltar */
                    while (i < blen && lp < ED_ROWS) {
                        if (i == cur) {
                            /* Imprimir el match resaltado carácter a carácter */
                            for (int pi = 0; pi < plen && i < blen; pi++, i++) {
                                hchar[0] = buf[i];
                                vga_print_color(hchar, VGA_BLACK, VGA_LIGHT_GREEN);
                            }
                            continue;
                        }
                        if (buf[i] == '\n') lp++;
                        vga_putchar(buf[i]);
                        i++;
                    }
                }

                /* Barra de estado de búsqueda */
                vga_print_color("\nBuscar: '", VGA_YELLOW, VGA_BLACK);
                vga_print_color(pat, VGA_WHITE, VGA_BLACK);
                vga_print_color("'  [", VGA_YELLOW, VGA_BLACK);
                vga_print_int(match_idx + 1);
                vga_print_color("/", VGA_YELLOW, VGA_BLACK);
                vga_print_int(match_count);
                vga_print_color("]", VGA_YELLOW, VGA_BLACK);

                unsigned char nc = kb_getchar();
                if (nc == 27 || nc == 6) break;          /* Esc o Ctrl+F: salir */
                if (nc == 'n' || nc == KEY_DOWN)
                    match_idx = (match_idx + 1) % match_count;
                else if (nc == 'N' || nc == KEY_UP)
                    match_idx = (match_idx - 1 + match_count) % match_count;
                /* cualquier otra tecla → salir */
                else break;
            }
            #undef MAX_MATCHES
            #undef SEARCH_MAX
            continue;
        }

        /* Navegación */
        if (c == KEY_LEFT) {
            if (cur > 0) cur--;
            continue;
        }
        if (c == KEY_RIGHT) {
            if (cur < blen) cur++;
            continue;
        }
        if (c == KEY_UP) {
            /* Subir una línea: buscar el '\n' anterior al cursor */
            int col = 0, p = cur - 1;
            while (p >= 0 && buf[p] != '\n') { p--; col++; }
            /* p apunta al '\n' de la línea anterior o a -1 */
            if (p < 0) { cur = 0; continue; }
            /* Ahora buscar el inicio de la línea anterior */
            int line_end = p;
            int line_start = line_end - 1;
            while (line_start >= 0 && buf[line_start] != '\n') line_start--;
            line_start++;
            int line_len = line_end - line_start;
            cur = line_start + (col < line_len ? col : line_len);
            continue;
        }
        if (c == KEY_DOWN) {
            /* Bajar una línea */
            int col = 0, p = cur - 1;
            while (p >= 0 && buf[p] != '\n') { p--; col++; }
            /* Encontrar el siguiente '\n' */
            int next_line = cur;
            while (next_line < blen && buf[next_line] != '\n') next_line++;
            if (next_line >= blen) { cur = blen; continue; }
            next_line++;   /* saltar el '\n' */
            int end = next_line;
            while (end < blen && buf[end] != '\n') end++;
            int line_len = end - next_line;
            cur = next_line + (col < line_len ? col : line_len);
            continue;
        }
        if (c == KEY_HOME) {
            while (cur > 0 && buf[cur-1] != '\n') cur--;
            continue;
        }
        if (c == KEY_END) {
            while (cur < blen && buf[cur] != '\n') cur++;
            continue;
        }

        /* Ignorar otras teclas especiales */
        if (c >= KEY_UP) continue;

        /* Backspace: borrar carácter antes del cursor */
        if (c == '\b') {
            if (cur > 0) {
                for (int i = cur - 1; i < blen - 1; i++) buf[i] = buf[i+1];
                blen--; cur--;
                buf[blen] = '\0';
            }
            continue;
        }

        /* Insertar carácter en posición cursor */
        if (blen < FS_MAX_CONTENT - 1) {
            for (int i = blen; i > cur; i--) buf[i] = buf[i-1];
            buf[cur] = (char)c;
            blen++; cur++;
            buf[blen] = '\0';
        }
    }
    #undef ED_ROWS
}


/* ── cat archivo — muestra contenido en pantalla ────────────────────────── */
void cmd_cat(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) { vga_println("Uso: cat archivo"); return; }

    int idx = resolve_path(name);
    if (idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(name); return;
    }
    FSNode *n = fs_get(idx);
    if (n->type != NODE_FILE) { vga_println("No es un archivo."); return; }

    const char *content = fs_read(idx);
    if (!content || content[0] == '\0') {
        vga_print_color("(archivo vacio)\n", VGA_DARK_GRAY, VGA_BLACK);
        return;
    }
    vga_println(content);
}


/* ── find nombre — busca en todo el FS ──────────────────────────────────── */
void find_recursive(int dir_idx, const char *target, int depth) {
    int iter = 0, child;
    while ((child = fs_next_child(dir_idx, &iter)) != -1) {
        FSNode *n = fs_get(child);
        /* Comparación simple: ¿contiene el nombre? */
        /* Usamos búsqueda por substring manual */
        const char *haystack = n->name;
        const char *needle   = target;
        int tlen = k_strlen(needle);
        int found = 0;
        for (int i = 0; haystack[i]; i++) {
            if (k_strncmp(haystack + i, needle, tlen) == 0) { found = 1; break; }
        }
        if (found) {
            char pathbuf[FS_PATH_MAX];
            fs_path(child, pathbuf, FS_PATH_MAX);
            vga_print_color(n->type == NODE_DIR ? "[DIR]  " : "[FILE] ",
                            n->type == NODE_DIR ? VGA_LIGHT_CYAN : VGA_LIGHT_GREEN,
                            VGA_BLACK);
            vga_println(pathbuf);
        }
        if (n->type == NODE_DIR) find_recursive(child, target, depth + 1);
    }
}

void cmd_find(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) { vga_println("Uso: find nombre"); return; }
    vga_print_color("Buscando '", VGA_DARK_GRAY, VGA_BLACK);
    vga_print_color(name, VGA_DARK_GRAY, VGA_BLACK);
    vga_print_color("'...\n", VGA_DARK_GRAY, VGA_BLACK);
    find_recursive(fs_root(), name, 0);
}


/* ── write archivo texto — escribe texto en un archivo (sobreescribe) ─────── */
void cmd_write(const char *line) {
    /* "write ARCHIVO TEXTO..." */
    const char *rest = arg1(line);
    /* Separar nombre del texto */
    int i = 0;
    while (rest[i] && rest[i] != ' ') i++;
    if (i == 0) { vga_println("Uso: write archivo texto"); return; }

    char fname[FS_MAX_NAME];
    int flen = i < FS_MAX_NAME-1 ? i : FS_MAX_NAME-1;
    for (int j = 0; j < flen; j++) fname[j] = rest[j];
    fname[flen] = '\0';

    const char *text = ltrim(rest + i);

    int idx = resolve_path(fname);
    if (idx < 0) {
        idx = fs_create(cwd, fname, NODE_FILE);
        if (idx < 0) { vga_println("No se pudo crear el archivo."); return; }
    }
    if (fs_get(idx)->type != NODE_FILE) { vga_println("No es un archivo."); return; }
    fs_write(idx, text);
    fs_save_to_disk();
    vga_print("Escrito en '"); vga_print(fname); vga_println("'.");
}


/* ── append archivo texto — anade texto al final de un archivo ───────────── */
void cmd_append(const char *line) {
    const char *rest = arg1(line);
    int i = 0;
    while (rest[i] && rest[i] != ' ') i++;
    if (i == 0) { vga_println("Uso: append archivo texto"); return; }

    char fname[FS_MAX_NAME];
    int flen = i < FS_MAX_NAME-1 ? i : FS_MAX_NAME-1;
    for (int j = 0; j < flen; j++) fname[j] = rest[j];
    fname[flen] = '\0';

    const char *text = ltrim(rest + i);

    int idx = resolve_path(fname);
    if (idx < 0) {
        idx = fs_create(cwd, fname, NODE_FILE);
        if (idx < 0) { vga_println("No se pudo crear el archivo."); return; }
    }
    if (fs_get(idx)->type != NODE_FILE) { vga_println("No es un archivo."); return; }

    /* Construir nuevo contenido = existente + '\n' + texto */
    static char newbuf[FS_MAX_CONTENT];
    k_memset(newbuf, 0, FS_MAX_CONTENT);
    const char *existing = fs_read(idx);
    int pos = 0;
    if (existing)
        while (existing[pos] && pos < FS_MAX_CONTENT - 2)
            { newbuf[pos] = existing[pos]; pos++; }
    if (pos > 0 && newbuf[pos-1] != '\n') newbuf[pos++] = '\n';
    int j = 0;
    while (text[j] && pos < FS_MAX_CONTENT - 1) newbuf[pos++] = text[j++];
    newbuf[pos] = '\0';
    fs_write(idx, newbuf);
    fs_save_to_disk();
    vga_print("Texto anadido a '"); vga_print(fname); vga_println("'.");
}


/* ── tree — árbol recursivo del FS desde cwd (o ruta dada) ──────────────────
   Uso: tree
        tree directorio
   Muestra el contenido en forma de árbol con sangría.
   ─────────────────────────────────────────────────────────────────────────── */
void cmd_tree_recurse(int node_idx, int depth) {
    int iter = 0, child;
    while ((child = fs_next_child(node_idx, &iter)) != -1) {
        FSNode *n = fs_get(child);
        /* Sangría: 2 espacios por nivel */
        for (int i = 0; i < depth * 2; i++) vga_putchar(' ');
        if (n->type == NODE_DIR) {
            vga_print_color("[+] ", VGA_LIGHT_CYAN, VGA_BLACK);
            vga_print_color(n->name, VGA_LIGHT_CYAN, VGA_BLACK);
            vga_putchar('\n');
            cmd_tree_recurse(child, depth + 1);
        } else {
            vga_print_color("[-] ", VGA_LIGHT_GREEN, VGA_BLACK);
            vga_println(n->name);
        }
    }
}

void cmd_tree(const char *line) {
    const char *arg = arg1(line);
    int start;
    if (k_strlen(arg) == 0) {
        start = cwd;
    } else {
        start = resolve_path(arg);
        if (start < 0) {
            vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
            vga_println(arg);
            return;
        }
        if (fs_get(start)->type != NODE_DIR) {
            vga_println("tree solo funciona con directorios.");
            return;
        }
    }
    /* Imprimir la raíz del árbol */
    char pathbuf[FS_PATH_MAX];
    fs_path(start, pathbuf, FS_PATH_MAX);
    vga_print_color(pathbuf, VGA_YELLOW, VGA_BLACK);
    vga_putchar('\n');
    cmd_tree_recurse(start, 1);
}


/* ── wordc — contar líneas, palabras y bytes de un archivo ──────────────────
   Uso: wordc archivo
   ─────────────────────────────────────────────────────────────────────────── */
void cmd_wordc(const char *line) {
    const char *name = arg1(line);
    if (k_strlen(name) == 0) { vga_println("Uso: wordc archivo"); return; }

    int idx = resolve_path(name);
    if (idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(name);
        return;
    }
    FSNode *n = fs_get(idx);
    if (n->type != NODE_FILE) { vga_println("wordc solo funciona con archivos."); return; }

    const char *content = fs_read(idx);
    if (!content) content = "";

    long lines = 0, words = 0, bytes = 0;
    int in_word = 0;

    for (int i = 0; content[i]; i++) {
        bytes++;
        if (content[i] == '\n') lines++;
        /* Separadores de palabra: espacio, tab, nueva línea */
        if (content[i] == ' ' || content[i] == '\t' || content[i] == '\n') {
            in_word = 0;
        } else {
            if (!in_word) { words++; in_word = 1; }
        }
    }

    vga_print_color("Lineas : ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_long(lines);  vga_putchar('\n');
    vga_print_color("Palabras: ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_long(words);  vga_putchar('\n');
    vga_print_color("Bytes  : ", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_long(bytes);  vga_putchar('\n');
}


/* ── head — primeras N líneas de un archivo ──────────────────────────────────
   Uso: head archivo [N]   (N por defecto = 10)
   ─────────────────────────────────────────────────────────────────────────── */
void cmd_head(const char *line) {
    const char *rest = arg1(line);
    if (k_strlen(rest) == 0) { vga_println("Uso: head archivo [N]"); return; }

    /* Extraer nombre de archivo (primer token) */
    char fname[FS_MAX_NAME];
    int fi = 0;
    while (rest[fi] && rest[fi] != ' ' && fi < FS_MAX_NAME - 1)
        { fname[fi] = rest[fi]; fi++; }
    fname[fi] = '\0';

    /* Número de líneas (segundo token, opcional) */
    int max_lines = 10;
    const char *nstr = ltrim(rest + fi);
    if (*nstr) {
        max_lines = 0;
        while (*nstr >= '0' && *nstr <= '9') max_lines = max_lines * 10 + (*nstr++ - '0');
        if (max_lines <= 0) max_lines = 10;
    }

    int idx = resolve_path(fname);
    if (idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(fname);
        return;
    }
    if (fs_get(idx)->type != NODE_FILE) { vga_println("head solo funciona con archivos."); return; }

    const char *content = fs_read(idx);
    if (!content || !*content) { vga_println("(archivo vacio)"); return; }

    int lines_printed = 0;
    for (int i = 0; content[i] && lines_printed < max_lines; i++) {
        vga_putchar(content[i]);
        if (content[i] == '\n') lines_printed++;
    }
    /* Asegurar salto de línea al final si el contenido no termina en \n */
    if (lines_printed < max_lines) vga_putchar('\n');
}


/* ── tail — últimas N líneas de un archivo ───────────────────────────────────
   Uso: tail archivo [N]   (N por defecto = 10)
   ─────────────────────────────────────────────────────────────────────────── */
void cmd_tail(const char *line) {
    const char *rest = arg1(line);
    if (k_strlen(rest) == 0) { vga_println("Uso: tail archivo [N]"); return; }

    char fname[FS_MAX_NAME];
    int fi = 0;
    while (rest[fi] && rest[fi] != ' ' && fi < FS_MAX_NAME - 1)
        { fname[fi] = rest[fi]; fi++; }
    fname[fi] = '\0';

    int max_lines = 10;
    const char *nstr = ltrim(rest + fi);
    if (*nstr) {
        max_lines = 0;
        while (*nstr >= '0' && *nstr <= '9') max_lines = max_lines * 10 + (*nstr++ - '0');
        if (max_lines <= 0) max_lines = 10;
    }

    int idx = resolve_path(fname);
    if (idx < 0) {
        vga_print_color("No encontrado: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_println(fname);
        return;
    }
    if (fs_get(idx)->type != NODE_FILE) { vga_println("tail solo funciona con archivos."); return; }

    const char *content = fs_read(idx);
    if (!content || !*content) { vga_println("(archivo vacio)"); return; }

    /* Contar total de líneas */
    int total_lines = 0;
    for (int i = 0; content[i]; i++)
        if (content[i] == '\n') total_lines++;

    /* Calcular desde qué línea empezar */
    int skip = total_lines - max_lines;
    if (skip < 0) skip = 0;

    /* Avanzar hasta la línea de inicio */
    int i = 0, cur_line = 0;
    while (content[i] && cur_line < skip) {
        if (content[i] == '\n') cur_line++;
        i++;
    }

    /* Imprimir desde ahí */
    for (; content[i]; i++) vga_putchar(content[i]);
    if (content[i-1] != '\n') vga_putchar('\n');
}


