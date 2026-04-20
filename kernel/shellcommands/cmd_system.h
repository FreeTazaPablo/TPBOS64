#ifndef CMD_SYSTEM_H
#define CMD_SYSTEM_H

/* echo, about, ver, memory, time, date, aboutcpu, fsinfo,
   history, clear/clean, reboot, poweroff                   */

void cmd_echo    (const char *line);
void cmd_about   (void);
void cmd_ver     (void);
void cmd_memory  (void);
void cmd_time    (void);
void cmd_date    (void);
void cmd_aboutcpu(void);
void cmd_fsinfo  (void);
void cmd_history (void);
void cmd_clear   (void);
void cmd_reboot  (void);
void cmd_poweroff(void);
void cmd_sleep   (const char *line);

#endif /* CMD_SYSTEM_H */
