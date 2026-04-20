#ifndef CMD_FS_H
#define CMD_FS_H

/* cat, write, append, find, copy, new, remove,
   move, rename, open, go, list, tree, wordc, head, tail */

void cmd_cat   (const char *line);
void cmd_write (const char *line);
void cmd_append(const char *line);
void cmd_find  (const char *line);
void cmd_copy  (const char *line);
void cmd_new   (const char *line);
void cmd_remove(const char *line);
void cmd_move  (const char *line);
void cmd_rename(const char *line);
void cmd_open  (const char *line);
void cmd_go    (const char *line);
void cmd_list  (void);
void cmd_tree  (const char *line);
void cmd_wordc (const char *line);
void cmd_head  (const char *line);
void cmd_tail  (const char *line);

#endif /* CMD_FS_H */
