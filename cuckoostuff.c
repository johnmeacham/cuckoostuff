/*
Copyright (c) 2014 John Meacham http://notanumber.net/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
#include <stdint.h>
#include <string.h>

#include "cuckoostuff.h"

/* Cuckoo stuff byte stuffing algorithm.
 *
 * Bytestream format
 *
 * At any point there are two escape codes active, e1 and e2. They are never
 * equal to each other or zero.
 *
 * The decoding rules are as follows:
 * e1 -> 0
 * e2 e1 -> e1  and replace e1 with next code in sequence.
 * e2 e2 -> e2  and replace e2 with next code in sequence.
 * e2 x | x not in {e1,e2} -> e2 x
 * x -> x
 *
 * the encoding rules are the opposite:
 * 0 -> e1
 * e1 -> e2 e1  and replace e1 with next code in sequence.
 * e2 x | x not in {e1,e2,0} -> e2 x
 * e2 -> e2 e2  and replace e2 with next code in sequence.
 * x -> x
 */

/*
 * Escape code iterator, generates a uniformly random sequence of bytes that are
 * used as the escape characters.
 */
uint8_t
next_ecode(struct ecode_generator_state *s, uint8_t verboten)
{
        do {
                uint8_t t = s->x ^ (s->x << 3);
                s->x = s->y; s->y = s->z; s->z = s->w;
                s->w = s->w ^ (s->w >> 5) ^ (t ^ (t >> 2));
        } while (!s->w || s->w == verboten);
        return s->w;
}

/*
 * The output buffer should be at least double the size of the input buffer for
 * the absolute worst case encoding. In practice, It will grow less than half a
 * percent. The input and output buffers may not overlap.
 */
unsigned
cuckoo_stuff(const uint8_t * in, unsigned len, uint8_t * out)
{
        unsigned o = 0;
        struct ecode_generator_state state = INIT_CODE_STATE;
        uint8_t e1 = INIT_E1;
        uint8_t e2 = INIT_E2;

        for (unsigned i = 0; i < len; i++) {
                if (in[i] == 0) {
                        out[o++] = e1;
                } else if (in[i] == e1) {
                        out[o++] = e2;
                        out[o++] = e1;
                        e1 = next_ecode(&state, e2);
                } else if (in[i] == e2) {
                        out[o++] = e2;
                        if (i + 1 >= len) {
                                continue;
                        }
                        uint8_t n = in[i + 1];
                        if (n == 0 || n == e1 || n == e2) {
                                out[o++] = e2;
                                e2 = next_ecode(&state, e1);
                        }
                } else
                        out[o++] = in[i];
        }
        return o;
}

/*
 * The output buffer should be at least the size of the input buffer.
 * In-place decoding is supported by making in and out equal.
 */
unsigned
cuckoo_unstuff(const uint8_t * in, unsigned len, uint8_t * out)
{
        unsigned o = 0;
        struct ecode_generator_state state = INIT_CODE_STATE;
        uint8_t e1 = INIT_E1;
        uint8_t e2 = INIT_E2;

        for (unsigned i = 0; i < len; i++) {
                if (in[i] == e1) {
                        out[o++] = 0;
                } else if (in[i] == e2 && i + 1 < len) {
                        if (in[i + 1] == e1) {
                                i++;
                                out[o++] = e1;
                                e1 = next_ecode(&state, e2);
                        } else if (in[i + 1] == e2) {
                                i++;
                                out[o++] = e2;
                                e2 = next_ecode(&state, e1);
                        } else
                                out[o++] = in[i];
                } else
                        out[o++] = in[i];
        }
        return o;
}

/* Returns number of bytes written to out. will always be one or two
 * coder_state should be initialized with INIT_STATE */
uint8_t
cuckoo_stuff_byte(struct coder_state *s, uint8_t byte, uint8_t out[2])
{
        out[0] = byte ? byte : s->e1;
        out[1] = byte;
        if (byte == s->e2) {
                s->e2 = next_ecode(&s->code, s->e1);
                return 2;
        } else if (byte == s->e1) {
                out[0] = s->e2;
                s->e1 = next_ecode(&s->code, s->e2);
                return 2;
        }
        return 1;
}

/* Returns number of bytes written to out.
 * will always return zero, one, or two.
 *
 * Pass in a byte value of zero (will never occur in a stuffed bytestream) to
 * indicate EOF. */
uint8_t
cuckoo_unstuff_byte(struct coder_state *s, uint8_t byte, uint8_t out[2])
{
        out[0] = byte;
        if (s->flag) {
                s->flag = false;
                if (byte == s->e1) {
                        s->e1 = next_ecode(&s->code, s->e2);
                        return 1;
                } else if (byte == s->e2) {
                        s->e2 = next_ecode(&s->code, s->e1);
                        return 1;
                } else {
                        out[0] = s->e2;
                        out[1] = byte;
                        return byte ? 2 : 1;
                }
        } else if (!byte || byte == s->e2) {
                s->flag = true;
                return 0;
        }
        if (byte == s->e1) {
                out[0] = 0;
        }
        return 1;
}

#ifdef DO_TEST
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

const uint8_t t0[0] = { };
const uint8_t t1[2] = { 0,0 };
const uint8_t t2[5] = { 0xff,0, 0xf8, 0xc1, 0 };
const uint8_t t3[9] = { 0x11, 0xff,0, 0xf8, 0, 0xf8, 0xc1, 0, 0x23 };
const uint8_t t4[22] = "Hello This is a test.";

const uint8_t t0_stuffed[0] = { };
const uint8_t t1_stuffed[2] = { 0xc1, 0xc1, };
const uint8_t t2_stuffed[7] = { 0xff, 0xc1, 0xf8, 0xf8, 0xa0, 0xc1, 0x5b, };
const uint8_t t3_stuffed[11] = { 0x11, 0xff, 0xc1, 0xf8, 0xf8, 0xc1, 0xf8, 0xa0, 0xc1, 0x5b, 0x23, };
const uint8_t t4_stuffed[22] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0x2e, 0xc1, };

void
test_vector(const uint8_t buf[], unsigned len, const char *name) {
                uint8_t out[len*2];
                unsigned olen = cuckoo_stuff(buf,len, out);
                printf("/* input is %u bytes long. */\n", len);
                printf("const uint8_t %s_stuffed[%u] = { ", name, olen);
                for (int i = 0; i < olen; i++) {
                        printf("0x%02x, ",out[i]);
                }
                printf("};\n");
                uint8_t back[olen];
                unsigned blen = cuckoo_unstuff(out,olen, back);
                assert(blen == len);
                assert(!memcmp(back,buf,blen));
}

int
main(int argc, char *argv[])
{
        test_vector(t0,sizeof(t0),"t0");
        test_vector(t1,sizeof(t1),"t1");
        test_vector(t2,sizeof(t2),"t2");
        test_vector(t3,sizeof(t3),"t3");
        test_vector(t4,sizeof(t4),"t4");
        return 0;
}
#endif
