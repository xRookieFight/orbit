#pragma once

void shell_run(void);
void shell_login(void);
void shell_request_logout(void);
int shell_history_count(void);
const char* shell_history_at(int index);
