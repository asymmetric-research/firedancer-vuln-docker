#ifndef HEADER_fd_src_app_quicfuzz_libafl_h
#define HEADER_fd_src_app_quicfuzz_libafl_h

#include "common.h"

#define MAX_EDGES_NUM 65536
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#ifndef __AFL_FUZZ_TESTCASE_LEN
ssize_t fuzz_len;
#define __AFL_FUZZ_TESTCASE_LEN fuzz_len
unsigned char fuzz_buf[2048];
#define __AFL_FUZZ_TESTCASE_BUF fuzz_buf
#define __AFL_FUZZ_INIT() void sync(void);
#define __AFL_LOOP(x) ((fuzz_len = read(0, fuzz_buf, sizeof(fuzz_buf))) > 0 ? 1 : 0)
#define __AFL_INIT() sync()
#endif

#endif
