#ifndef STATSTUFF_H
#define STATSTUFF_H

#include <stdint.h>
#include <stdbool.h>

#define INIT_E1 0xC1
#define INIT_E2 0xF8
#define INIT_CODE_STATE {  .x = 21, .y = 229, .z = 181, .w = 51 }

struct ecode_generator_state { uint8_t x, y, z, w; };
uint8_t next_ecode(struct ecode_generator_state *s, uint8_t verboten);

struct coder_state {
        struct ecode_generator_state code;
        uint8_t e1, e2;
        bool flag;
};

#define INIT_STATE { .code = INIT_CODE_STATE, .e1 = INIT_E1, .e2 = INIT_E2, .flag = false }
#define INIT_STATE_VALUE (struct coder_state)INIT_STATE

unsigned cuckoo_stuff(const uint8_t * in, unsigned len, uint8_t * out);
unsigned cuckoo_unstuff(const uint8_t * in, unsigned len, uint8_t * out);
uint8_t cuckoo_stuff_byte(struct coder_state *s, uint8_t byte, uint8_t out[2]);
uint8_t cuckoo_unstuff_byte(struct coder_state *s, uint8_t byte, uint8_t out[2]);

#endif
