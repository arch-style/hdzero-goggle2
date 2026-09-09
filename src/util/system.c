#include "system.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log/log.h"
#include "util/filesystem.h"
#include "util/time.h"

// Every one of these forks a shell. On this SoC that is not free, and the
// menu/video switch path runs several, so log how long each one took.
//
// time_ms() rather than gettimeofday(): rtc_init() moves the wall clock from
// 1970 to 2026 partway through start-up, and a wall-clock measurement that
// straddles it reads as tens of millions of milliseconds. The scripts either
// side of rtc_init() are exactly the ones worth timing.

int system_exec(const char *command) {
    LOGI("System Execute: %s", command);

    uint32_t start = time_ms();
    int retval = system(command);

    LOGI("System Execute: %ums for %s", time_ms() - start, command);

    return retval;
}

int system_script(const char *command) {
    LOGI("System Script: %s", command);

    // basename may edit argument
    const char *script = fs_basename(command);

    // Modify command to log std out and error to a temporary file
    char buffer[256];
    snprintf(buffer,
             sizeof(buffer), "%s > /tmp/%s.log 2>&1",
             command,
             script);
    uint32_t start = time_ms();
    int retval = system(buffer);

    LOGI("System Script: %ums for %s", time_ms() - start, command);

    // Read contents of temporary file and use logging framework
    snprintf(buffer, sizeof(buffer), "/tmp/%s.log", script);
    FILE *fp = fopen(buffer, "r");
    if (fp) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read = 0;
        while ((read = getline(&line, &len, fp)) != -1) {
            line[strcspn(line, "\r\n")] = 0;
            LOGD("%s", line);
        }
        if (line) {
            free(line);
        }
        fclose(fp);
    }

    return retval;
}
