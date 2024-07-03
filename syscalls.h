#ifndef _syscalls_h_
#define _syscasll_h_

#include <stdint.h>

typedef struct stdout_ops_s {
    void (*init)(void);
    uint32_t (*available)(void);
    uint32_t (*read)(uint8_t *buf, uint32_t len);
    uint32_t (*write)(const uint8_t *buf, uint32_t len);
}stdout_ops_t;

void redirect_stdout(stdout_ops_t *ops);
stdout_ops_t * get_stdout_redirect(void);

#endif