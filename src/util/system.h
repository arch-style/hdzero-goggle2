#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Log this thread's kernel id under a name. The i2c reports name the threads
// that were holding and waiting for a bus, and a bare number is not an answer
// to "who". Called once at the top of each thread.
void log_thread_id(const char *name);

int system_exec(const char *command);
int system_script(const char *script);

#ifdef __cplusplus
}
#endif
