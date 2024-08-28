/*
 * Asterisk -- An open source telephony toolkit.
 */

/*! \file
 *
 * \brief Translate between signed linear and Vocal EVS codec
 *
 * \ingroup codecs
 */

/*** MODULEINFO
	<depend>vevs</depend>
 ***/

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>

#include "asterisk.h"

#include "asterisk/translate.h"
#include "asterisk/rtp_engine.h"
#include "asterisk/module.h"
#include "asterisk/linkedlists.h"

#include <asterisk/vevs.h>
/* Sample frame data */
#include "asterisk/slin.h"
#include "ex_vevs.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#if 0
// minimum set of defines to make mockup happy
#define EVS_BW_NB  0u
#define EVS_BW_WB  1u
#define EVS_SF_8K  8000u
#define EVS_SF_16K 16000u
#define EVS_BR_5k90  5900u


#define BUFFER_SAMPLES	5760



typedef unsigned char uint8;
typedef unsigned short uint16;
typedef signed short sint15;
typedef unsigned int uint32;
typedef signed int sint31;
typedef unsigned long long uint64;
typedef long long sint63;
typedef uint16 vbool;

typedef void *(*xvocal_allocate_data)(void);
typedef void (*xvocal_destroy_data)(void *vocal_data);
typedef uint32 (*xvocal_get_data_size)(void);
typedef void (*xvocal_set_data)(void *vocal_data);
typedef int (*xvocal_evs_init_coder)(uint16 bandwidth, uint16 sampling_frequency, uint32 bitrate, vbool dtx_enable);
typedef int (*xvocal_evs_process_coder)(uint8 *enc_data, uint16 enc_data_size, const sint15 *enc_samples,
                                        uint16 sample_count);
typedef int (*xvocal_evs_close_coder)(void);
typedef int (*xvocal_evs_init_decoder)(uint16 sampling_frequency);
typedef int (*xvocal_evs_process_decoder)(sint15 *dec_samples, uint16 max_sample_count, uint8 *dec_data,
                                          uint16 dec_data_bit_count, vbool bad_frame);
typedef int (*xvocal_evs_close_decoder)(void);

#define STR(x) "\"" #x "\""
#define IMP(xx)                       \
  char *xx##_old_err = dlerror();     \
  xx = (x##xx)dlsym(lib_handle, #xx); \
  xx##_old_err = dlerror();           \
  if (xx##_old_err) {                 \
    perror(xx##_old_err);             \
    exit(1);                          \
  }

#define MOCK(xx) xx = mock_##xx;

#define DD(xx) static x##xx xx;

DD(vocal_allocate_data)
DD(vocal_destroy_data)
DD(vocal_set_data)

DD(vocal_evs_init_coder)
DD(vocal_evs_process_coder)
DD(vocal_evs_close_coder)

DD(vocal_evs_init_decoder)
DD(vocal_evs_process_decoder)
DD(vocal_evs_close_decoder)

static void *lib_handle = 0;

static void *mock_vocal_allocate_data(void) { return (void *)4321; }
static void mock_vocal_destroy_data(void *vocal_data) { return; }
static uint32 mock_vocal_get_data_size(void) { return 1234; }
static void mock_vocal_set_data(void *vocal_data) { return; }
static int mock_vocal_evs_init_coder(uint16 bandwidth, uint16 sampling_frequency, uint32 bitrate, vbool dtx_enable) {
  return 320;
}
static int mock_vocal_evs_process_coder(uint8 *enc_data, uint16 enc_data_size, const sint15 *enc_samples,
                                        uint16 sample_count) {
  return 56; /* prim 2.8 */
}
static int mock_vocal_evs_close_coder(void) { return 0; }
static int mock_vocal_evs_init_decoder(uint16 sampling_frequency) { return 320; }
static int mock_vocal_evs_process_decoder(sint15 *dec_samples, uint16 max_sample_count, uint8 *dec_data,
                                          uint16 dec_data_bit_count, vbool bad_frame) {
  return 320;
}
static int mock_vocal_evs_close_decoder(void) { return 0; }

static int w_alloc_vocal(void) {
  char *errs = dlerror(); // flush
  lib_handle = dlopen("libvocal.so", RTLD_LOCAL | RTLD_NOW);
  errs = dlerror();
  if (errs || !lib_handle) {
    perror(dlerror());
    ast_log(LOG_ERROR, "EVS CODEC NOT FOUND: USING MOCKUP!\n");
    // exit(1);
    MOCK(vocal_allocate_data)
    MOCK(vocal_destroy_data)
    MOCK(vocal_set_data)

    MOCK(vocal_evs_init_coder)
    MOCK(vocal_evs_process_coder)
    MOCK(vocal_evs_close_coder)

    MOCK(vocal_evs_init_decoder)
    MOCK(vocal_evs_process_decoder)
    MOCK(vocal_evs_close_decoder)
  } else {
    IMP(vocal_allocate_data)
    IMP(vocal_destroy_data)
    IMP(vocal_set_data)

    IMP(vocal_evs_init_coder)
    IMP(vocal_evs_process_coder)
    IMP(vocal_evs_close_coder)

    IMP(vocal_evs_init_decoder)
    IMP(vocal_evs_process_decoder)
    IMP(vocal_evs_close_decoder)
  }
  ast_log(LOG_DEBUG, "funcptr init!\n");
  return 0;
}

static int w_close_vocal(void) {
  dlclose(lib_handle);
  lib_handle = 0;
  ast_log(LOG_DEBUG, "closed!\n");
  return 0;
}

// static void* vdata = (void*)0; // mutex!
static int dl_refcount = 0; // mutex!

static void maybe_load_codeclib(void) {
  if (dl_refcount == 0 && lib_handle == 0) {
    w_alloc_vocal();
  }
  dl_refcount++;
}
static void maybe_unload_codeclib(void) {
  dl_refcount--;
  if (dl_refcount == 0 && lib_handle != 0) {
    w_close_vocal();
  }
}
#else
#define maybe_load_codeclib()
#define maybe_unload_codeclib()
#endif

enum CMR_T_000 {
  NB_5_9_VBR_ = 0b0000,
  NB_7_2 = 0b0001,
  NB_8_0 = 0b0010,
  NB_9_6 = 0b0011,
  NB_13_2 = 0b0100,
  NB_16_4 = 0b0101,
  NB_24_4 = 0b0110,
};
const char *CMR_T_000_str[] = {
    "NB_5_9_VBR_",
    "NB_7_2",
    "NB_8_0",
    "NB_9_6",
    "NB_13_2",
    "NB_16_4",
    "NB_24_4",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
};

unsigned char CMR_T_000_valid[] = {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const char *get_CMR_T_000_str(unsigned char v) {
  if (CMR_T_000_valid[v & 0xf])
    return CMR_T_000_str[v];
  return "";
}

enum CMR_T_010 {
  WB_5_9_VBR_ = 0b0000,
  WB_7_2 = 0b0001,
  WB_8 = 0b0010,
  WB_9_6 = 0b0011,
  WB_13_2 = 0b0100,
  WB_16_4 = 0b0101,
  WB_24_4 = 0b0110,
  WB_32 = 0b0111,
  WB_48 = 0b1000,
  WB_64 = 0b1001,
  WB_96 = 0b1010,
  WB_128 = 0b1011,
};
const char *CMR_T_010_str[] = {
    "WB_5_9_VBR_",
    "WB_7_2",
    "WB_8",
    "WB_9_6",
    "WB_13_2",
    "WB_16_4",
    "WB_24_4",
    "WB_32",
    "WB_48",
    "WB_64",
    "WB_96",
    "WB_128",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
};

unsigned char CMR_T_010_valid[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};

static const char *get_CMR_T_010_str(unsigned char v) {
  if (CMR_T_010_valid[v & 0xf])
    return CMR_T_010_str[v];
  return "";
}

enum CMR_T_001 {
  IO_8_85 = 0b0001,
  IO_12_65 = 0b0010,
  IO_14_25 = 0b0011,
  IO_15_85 = 0b0100,
  IO_18_25 = 0b0101,
  IO_19_85 = 0b0110,
  IO_23_05 = 0b0111,
  IO_23_85 = 0b1000,
};
const char *CMR_T_001_str[] = {
    "IO_8_85",           "IO_12_65",          "IO_14_25",          "IO_15_85",          "IO_18_25",
    "IO_19_85",          "IO_23_05",          "IO_23_85",          "Not_used_Not_used", "Not_used_Not_used",
    "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used",
};

unsigned char CMR_T_001_valid[] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};

static const char *get_CMR_T_001_str(unsigned char v) {
  if (CMR_T_001_valid[v & 0xf])
    return CMR_T_001_str[v];
  return "";
}

enum CMR_T_011 {
  SWB_9_6 = 0b0011,
  SWB_13_2 = 0b0100,
  SWB_16_4 = 0b0101,
  SWB_24_4 = 0b0110,
  SWB_32 = 0b0111,
  SWB_48 = 0b1000,
  SWB_64 = 0b1001,
  SWB_96 = 0b1010,
  SWB_128 = 0b1011,
};
const char *CMR_T_011_str[] = {
    "Not_used_Not_used",
    "Not_used_Not_used",
    "SWB_9_6",
    "SWB_13_2",
    "SWB_16_4",
    "SWB_24_4",
    "SWB_32",
    "SWB_48",
    "SWB_64",
    "SWB_96",
    "SWB_128",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
};

unsigned char CMR_T_011_valid[] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};

static const char *get_CMR_T_011_str(unsigned char v) {
  if (CMR_T_011_valid[v & 0xf])
    return CMR_T_011_str[v];
  return "";
}

enum CMR_T_100 {
  FB_16_4 = 0b0101,
  FB_24_4 = 0b0110,
  FB_32 = 0b0111,
  FB_48 = 0b1000,
  FB_64 = 0b1001,
  FB_96 = 0b1010,
  FB_128 = 0b1011,
};
const char *CMR_T_100_str[] = {
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "FB_16_4",
    "FB_24_4",
    "FB_32",
    "FB_48",
    "FB_64",
    "FB_96",
    "FB_128",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
    "Not_used_Not_used",
};

unsigned char CMR_T_100_valid[] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};

static const char *get_CMR_T_100_str(unsigned char v) {
  if (CMR_T_100_valid[v & 0xf])
    return CMR_T_100_str[v];
  return "";
}

enum CMR_T_110 {
  SWB_13_2_CA_L_O2 = 0b0000,
  SWB_13_2_CA_L_O3 = 0b0001,
  SWB_13_2_CA_L_O5 = 0b0010,
  SWB_13_2_CA_L_O7 = 0b0011,
  SWB_13_2_CA_H_O2 = 0b0100,
  SWB_13_2_CA_H_O3 = 0b0101,
  SWB_13_2_CA_H_O5 = 0b0110,
  SWB_13_2_CA_H_O7 = 0b0111,
};
const char *CMR_T_110_str[] = {
    "SWB_13_2_CA_L_O2",  "SWB_13_2_CA_L_O3",  "SWB_13_2_CA_L_O5",  "SWB_13_2_CA_L_O7",
    "SWB_13_2_CA_H_O2",  "SWB_13_2_CA_H_O3",  "SWB_13_2_CA_H_O5",  "SWB_13_2_CA_H_O7",
    "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used",
    "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used",
};

unsigned char CMR_T_110_valid[] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

static const char *get_CMR_T_110_str(unsigned char v) {
  if (CMR_T_110_valid[v & 0xf])
    return CMR_T_110_str[v];
  return "";
}

enum CMR_T_101 {
  WB_13_2_CA_L_O3 = 0b0001,
  WB_13_2_CA_L_O5 = 0b0010,
  WB_13_2_CA_L_O7 = 0b0011,
  WB_13_2_CA_H_O2 = 0b0100,
  WB_13_2_CA_H_O3 = 0b0101,
  WB_13_2_CA_H_O5 = 0b0110,
  WB_13_2_CA_H_O7 = 0b0111,
};
const char *CMR_T_101_str[] = {
    "WB_13_2_CA_L_O3",   "WB_13_2_CA_L_O5",   "WB_13_2_CA_L_O7",   "WB_13_2_CA_H_O2",   "WB_13_2_CA_H_O3",
    "WB_13_2_CA_H_O5",   "WB_13_2_CA_H_O7",   "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used",
    "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used", "Not_used_Not_used",
};

unsigned char CMR_T_101_valid[] = {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

static const char *get_CMR_T_101_str(unsigned char v) {
  if (CMR_T_101_valid[v & 0xf])
    return CMR_T_101_str[v];
  return "";
}

enum CMR_T_111 { EMPTY_DUMMY = 0 };
const char *CMR_T_111_str[] = {
    "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved",
    "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved",
    "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved", "Not_used_Reserved", "Not_used_NO_REQ",
};

unsigned char CMR_T_111_valid[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const char *get_CMR_T_111_str(unsigned char v) {
  if (CMR_T_111_valid[v & 0xf])
    return CMR_T_111_str[v];
  return "";
}

enum CMR_T {
  CMR_T_000 = 0b000,
  CMR_T_001 = 0b001,
  CMR_T_010 = 0b010,
  CMR_T_011 = 0b011,
  CMR_T_100 = 0b100,
  CMR_T_101 = 0b101,
  CMR_T_110 = 0b110,
  CMR_T_111 = 0b111,
};

enum TOC_MODE_0 {
  Primary_2_8_kbps = 0b0000,
  Primary_7_2_kbps = 0b0001,
  Primary_8_0_kbps = 0b0010,
  Primary_9_6_kbps = 0b0011,
  Primary_13_2_kbps = 0b0100,
  Primary_16_4_kbps = 0b0101,
  Primary_24_4_kbps = 0b0110,
  Primary_32_0_kbps = 0b0111,
  Primary_48_0_kbps = 0b1000,
  Primary_64_0_kbps = 0b1001,
  Primary_96_0_kbps = 0b1010,
  Primary_128_0_kbps = 0b1011,
  Primary_2_4kbps_SID = 0b1100,
  SPEECH_LOST = 0b1110,
  NO_DATA0 = 0b1111,
};
const char *TOC_MODE_0_str[] = {
    "Primary_2_8_kbps",    "Primary_7_2_kbps",  "Primary_8_0_kbps",  "Primary_9_6_kbps",
    "Primary_13_2_kbps",   "Primary_16_4_kbps", "Primary_24_4_kbps", "Primary_32_0_kbps",
    "Primary_48_0_kbps",   "Primary_64_0_kbps", "Primary_96_0_kbps", "Primary_128_0_kbps",
    "Primary_2_4kbps_SID", "For_future_use",    "SPEECH_LOST",       "NO_DATA",
};

unsigned char TOC_MODE_0_valid[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1};

static const char *get_TOC_MODE_0_str(unsigned char v) {
  if (TOC_MODE_0_valid[v & 0xf])
    return TOC_MODE_0_str[v];
  return "";
}

enum TOC_MODE_1 {
  AMR_WB_IO_6_6_kbps = 0b0000,
  AMR_WB_IO_8_85_kbps = 0b0001,
  AMR_WB_IO_12_65_kbps = 0b0010,
  AMR_WB_IO_14_25_kbps = 0b0011,
  AMR_WB_IO_15_85_kbps = 0b0100,
  AMR_WB_IO_18_25_kbps = 0b0101,
  AMR_WB_IO_19_85_kbps = 0b0110,
  AMR_WB_IO_23_05_kbps = 0b0111,
  AMR_WB_IO_23_85_kbps = 0b1000,
  AMR_WB_IO_2_0_kbps_SID = 0b1001,
  SPEECH_LOST0 = 0b1110,
  NO_DATA1 = 0b1111,
};
const char *TOC_MODE_1_str[] = {
    "AMR_WB_IO_6_6_kbps",   "AMR_WB_IO_8_85_kbps",    "AMR_WB_IO_12_65_kbps", "AMR_WB_IO_14_25_kbps",
    "AMR_WB_IO_15_85_kbps", "AMR_WB_IO_18_25_kbps",   "AMR_WB_IO_19_85_kbps", "AMR_WB_IO_23_05_kbps",
    "AMR_WB_IO_23_85_kbps", "AMR_WB_IO_2_0_kbps_SID", "For_future_use",       "For_future_use",
    "For_future_use",       "For_future_use",         "SPEECH_LOST",          "NO_DATA",
};

unsigned char TOC_MODE_1_valid[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1};

static const char *get_TOC_MODE_1_str(unsigned char v) {
  if (TOC_MODE_1_valid[v & 0xf])
    return TOC_MODE_1_str[v];
  return "";
}

enum COMPACT_T {
  INVALID_COMPACT_MODE,
  EVS_Primary,
  EVS_AMR_WB_IO,
};

static enum COMPACT_T get_comp_type(int bitlen, unsigned char first_pl_byte, int *out_br) {
  switch (bitlen) {
    case 48:
      *out_br = Primary_2_4kbps_SID;
      return EVS_Primary;
      break;
    case 56:
      {
        if (first_pl_byte & 0x80) {
          // EVS AMR-WB IO SID frame + full header + CMR!
          return INVALID_COMPACT_MODE;
        }
        *out_br = Primary_2_8_kbps;
        return EVS_Primary;
        break;
      }
    case 136:
      *out_br = AMR_WB_IO_6_6_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 144:
      *out_br = Primary_7_2_kbps;
      return EVS_Primary;
      break;
    case 160:
      *out_br = Primary_8_0_kbps;
      return EVS_Primary;
      break;
    case 184:
      *out_br = AMR_WB_IO_8_85_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 192:
      *out_br = Primary_9_6_kbps;
      return EVS_Primary;
      break;
    case 256:
      *out_br = AMR_WB_IO_12_65_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 264:
      *out_br = Primary_13_2_kbps;
      return EVS_Primary;
      break;
    case 288:
      *out_br = AMR_WB_IO_14_25_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 320:
      *out_br = AMR_WB_IO_15_85_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 328:
      *out_br = Primary_16_4_kbps;
      return EVS_Primary;
      break;
    case 368:
      *out_br = AMR_WB_IO_18_25_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 400:
      *out_br = AMR_WB_IO_19_85_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 464:
      *out_br = AMR_WB_IO_23_05_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 480:
      *out_br = AMR_WB_IO_23_85_kbps;
      return EVS_AMR_WB_IO;
      break;
    case 488:
      *out_br = Primary_24_4_kbps;
      return EVS_Primary;
      break;
    case 640:
      *out_br = Primary_32_0_kbps;
      return EVS_Primary;
      break;
    case 960:
      *out_br = Primary_48_0_kbps;
      return EVS_Primary;
      break;
    case 1280:
      *out_br = Primary_64_0_kbps;
      return EVS_Primary;
      break;
    case 1920:
      *out_br = Primary_96_0_kbps;
      return EVS_Primary;
      break;
    case 2560:
      *out_br = Primary_128_0_kbps;
      return EVS_Primary;
      break;
    default: return INVALID_COMPACT_MODE;
  }
}

static int lookup_fh_sizes_evs(int tocval) {
  switch (tocval) {
    case 0x0: return 56;
    case 0x1: return 144;
    case 0x2: return 160;
    case 0x3: return 192;
    case 0x4: return 264;
    case 0x5: return 328;
    case 0x6: return 488;
    case 0x7: return 640;
    case 0x8: return 960;
    case 0x9: return 1280;
    case 0xa: return 1920;
    case 0xb: return 2560;
    case 0xc: return 48;
    default:  return 0;
  }
}

static int lookup_fh_sizes_amr(int tocval) {
  switch (tocval) {
    case 0x0: return 136;
    case 0x1: return 184;
    case 0x2: return 256;
    case 0x3: return 288;
    case 0x4: return 320;
    case 0x5: return 368;
    case 0x6: return 400;
    case 0x7: return 464;
    case 0x8: return 480;
    case 0x9: return 56;
    default:  return 0;
  }
}

uint8_t cmr_ht_mask = 0b10000000;
uint8_t cmr_T_mask = 0b01110000;
uint8_t cmr_D_mask = 0b00001111;

uint8_t toc_ht_mask = 0b10000000;
uint8_t toc_F_mask = 0b01000000;
uint8_t toc_FT_mask = 0b00111111;
uint8_t toc_EVS_mask = 0b00100000;
uint8_t toc_Q_mask = 0b00010000;

#define strvalid(e, d) get_##e##_str(d)
static void pr_T_D(unsigned char C) {
  uint8_t cmr_ht = C & cmr_ht_mask;
  uint8_t cmr_T = (C & cmr_T_mask) >> 4;
  uint8_t cmr_D = C & cmr_D_mask;
  const char *msg = "";

  switch (cmr_T) {
    case CMR_T_000: msg = strvalid(CMR_T_000, cmr_D); break;
    case CMR_T_001: msg = strvalid(CMR_T_001, cmr_D); break;
    case CMR_T_010: msg = strvalid(CMR_T_010, cmr_D); break;
    case CMR_T_011: msg = strvalid(CMR_T_011, cmr_D); break;

    case CMR_T_100: msg = strvalid(CMR_T_100, cmr_D); break;
    case CMR_T_101: msg = strvalid(CMR_T_101, cmr_D); break;
    case CMR_T_110: msg = strvalid(CMR_T_110, cmr_D); break;
    case CMR_T_111: msg = strvalid(CMR_T_111, cmr_D); break;
  }
  ast_log(LOG_DEBUG, "%s -> ", msg);
}

static void pr_TOC(unsigned char C) {
  uint8_t toc_ht = C & toc_ht_mask;
  uint8_t toc_F = (C & toc_F_mask) >> 6;
  uint8_t toc_FT = C & toc_FT_mask;
  const char *msg = "";
  // assert( (C & toc_F_mask) == 0);

  switch (toc_FT & toc_EVS_mask) {
    case 0: msg = get_TOC_MODE_0_str(toc_FT); break;
    case 1: msg = get_TOC_MODE_1_str(toc_FT); break;
  }
  ast_log(LOG_DEBUG, "%s\n", msg);
}
#pragma GCC diagnostic pop

/* Encoder and decoder instances */

typedef struct evs_enc_ctx_t{
	void* vdata;
  int sock;
} evs_enc_ctx_t;

typedef struct evs_dec_ctx_t{
	void* vdata;
  int sock;
} evs_dec_ctx_t;

struct evs_enc_pvt {
	evs_enc_ctx_t enc;		/* Encoder states */
	int chunk;			/* Size of chunk to encode (in samples) */
	bool last_was_silent;
	int16_t buf[BUFFER_SAMPLES];	/* Buffer to store received uncompressed audio */
};

struct evs_dec_pvt {
	evs_dec_ctx_t dec;		/* Decoder states */
	int chunk;			/* Size of chunk to decode (in samples) */
};

#if 0
static int wrap_vocal_evs_init_coder(evs_enc_ctx_t *ctx, uint16 bandwidth, uint16 sampling_frequency, uint32 bitrate,
                                     vbool dtx_enable) {
  ctx->vdata = vocal_allocate_data();
  if (ctx->vdata == NULL) {
    return -1;
  }
  vocal_set_data(ctx->vdata);
  int r = vocal_evs_init_coder(bandwidth, sampling_frequency, bitrate, dtx_enable);
  return r;
}
static int wrap_vocal_evs_process_coder(evs_enc_ctx_t *ctx, uint8 *enc_data, uint16 enc_data_size,
                                        const sint15 *enc_samples, uint16 sample_count) {
  vocal_set_data(ctx->vdata);
  int r = vocal_evs_process_coder(enc_data, enc_data_size, enc_samples, sample_count);
  return r;
}
static int wrap_vocal_evs_close_coder(evs_enc_ctx_t *ctx) {
  vocal_evs_close_coder();
  vocal_destroy_data(ctx->vdata);
  return 0;
}

static int wrap_vocal_evs_init_decoder(evs_dec_ctx_t *ctx, uint16 sampling_frequency) {
  ctx->vdata = vocal_allocate_data();
  if (ctx->vdata == NULL) {

    return -1;
  }
  vocal_set_data(ctx->vdata);
  int r = vocal_evs_init_decoder(sampling_frequency);
  return r;
}

static int wrap_vocal_evs_process_decoder(evs_dec_ctx_t *ctx, sint15 *dec_samples, uint16 max_sample_count,
                                          uint8 *dec_data, uint16 dec_data_size, bool bad_frame) {
  vocal_set_data(ctx->vdata);
  int r = vocal_evs_process_decoder(dec_samples, max_sample_count, dec_data, dec_data_size, bad_frame);
  return r;
}

static int wrap_vocal_evs_close_decoder(evs_dec_ctx_t *ctx) {
  vocal_evs_close_decoder();
  vocal_destroy_data(ctx->vdata);
  return 0;
}
#else
#include <sys/socket.h>
#include <sys/un.h>

static int codec_init_conn(int *sock) {
  int ret = -1;
  int ret_retry_counter = 0;
  struct sockaddr_un name;
  memset(&name, 0, sizeof(struct sockaddr_un));
  name.sun_family = AF_UNIX;
  strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);

  *sock = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (*sock == -1) {
    perror("socket");
    return -1;
  }

  struct timeval tv = {.tv_sec = 0, .tv_usec = 100 * 1000};
  ret = setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv));
  if (ret == -1) { perror("sso"); }
  ret = setsockopt(*sock, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(tv));
  if (ret == -1) { perror("sso"); }

  // fcntl(ctx->sock, F_SETFL, fcntl(ctx->sock, F_GETFL, 0) | O_NONBLOCK);
  ret = connect(*sock, (const struct sockaddr *)&name, sizeof(struct sockaddr_un));
  while (ret != 0 && ret_retry_counter < 20) {
    ret_retry_counter++;
    // usleep(100000);
    ret = connect(*sock, (const struct sockaddr *)&name, sizeof(struct sockaddr_un));
  }
  if (ret < 0) {
    perror("connect");
    close(*sock);
    return -1;
    // return EXIT_FAILURE;
  }
  // fcntl(ctx->sock, F_SETFL, fcntl(ctx->sock, F_GETFL, 0) & ~O_NONBLOCK);
  return ret;
}

static int codec_sndrcv(int sock, Codec_Message *m) {
  int ret = -1;
  do {
    ret = send(sock, (void *)m, sizeof(Codec_Message), MSG_NOSIGNAL);
  } while (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
  if (ret == -1) {
    perror("send");
    close(sock);
    return -1;
  }

  if (m->type == MSG_TYPE_DEALLOC_D || m->type == MSG_TYPE_DEALLOC_E) return 0;

  do {
    ret = recv(sock, (void *)m, sizeof(Codec_Message), MSG_NOSIGNAL);
  } while (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
  if (ret == -1) {
    perror("recv");
    close(sock);
    return -1;
  }

  return m->ret;
}

static int wrap_vocal_evs_init_coder(evs_enc_ctx_t *ctx, uint16 bandwidth, uint16 sampling_frequency, uint32 bitrate,
				     bool dtx_enable) {
  if (codec_init_conn(&ctx->sock) != 0) return -1;

  Codec_Message m = {.type = MSG_TYPE_ALLOC_E,
		     .fpars.init_coder.bandwidth = bandwidth,
		     .fpars.init_coder.sampling_frequency = sampling_frequency,
		     .fpars.init_coder.bitrate = bitrate,
		     .fpars.init_coder.dtx_enable = dtx_enable};
  return codec_sndrcv(ctx->sock, &m);
}
static int wrap_vocal_evs_process_coder(evs_enc_ctx_t *ctx, uint8 *enc_data, uint16 enc_data_size,
					const sint15 *enc_samples, uint16 sample_count) {
  Codec_Message m = {.type = MSG_TYPE_PROCESS_E,
		     .fpars.process_coder.enc_data = enc_data,
		     .fpars.process_coder.enc_data_size = enc_data_size,
		     .fpars.process_coder.enc_samples = enc_samples,
		     .fpars.process_coder.sample_count = sample_count};
  memcpy(m.data.b, enc_samples, sample_count * sizeof(sint15));
  int rv = codec_sndrcv(ctx->sock, &m);
  if (rv > 0) memcpy(enc_data, m.data.b, rv / 8);
  return rv;
}
static int wrap_vocal_evs_close_coder(evs_enc_ctx_t *ctx) {
  Codec_Message m = {.type = MSG_TYPE_DEALLOC_E};
  int r = codec_sndrcv(ctx->sock, &m);
  if (r) close(ctx->sock);
  return r;
}

static int wrap_vocal_evs_init_decoder(evs_dec_ctx_t *ctx, uint16 sampling_frequency) {
  if (codec_init_conn(&ctx->sock) != 0) return -1;

  Codec_Message m = {
      .type = MSG_TYPE_ALLOC_D,
      .fpars.init_decoder.sampling_frequency = sampling_frequency,
  };
  return codec_sndrcv(ctx->sock, &m);
}

static int wrap_vocal_evs_process_decoder(evs_dec_ctx_t *ctx, sint15 *dec_samples, uint16 max_sample_count,
					  uint8 *dec_data, uint16 dec_data_size, bool bad_frame) {
  Codec_Message m = {
      .type = MSG_TYPE_PROCESS_D,
      .fpars.process_decoder.dec_samples = dec_samples,
      .fpars.process_decoder.max_sample_count = max_sample_count,
      .fpars.process_decoder.dec_data = dec_data,
      .fpars.process_decoder.dec_data_size = dec_data_size,
      .fpars.process_decoder.bad_frame = bad_frame,
  };
  if (dec_data_size) memcpy(m.data.b, dec_data, dec_data_size / 8);
  int rv = codec_sndrcv(ctx->sock, &m);
  memcpy(dec_samples, m.data.b, rv * sizeof(short));
  return rv;
}

static int wrap_vocal_evs_close_decoder(evs_dec_ctx_t *ctx) {
  Codec_Message m = {.type = MSG_TYPE_DEALLOC_D};
  int r = codec_sndrcv(ctx->sock, &m);
  if (r) close(ctx->sock);
  return r;
}
#endif

/* Create and destroy codec intances */

static int evs_enc_new(struct ast_trans_pvt *pvt) {
  struct evs_enc_pvt *enc = pvt->pvt;
  const unsigned int sample_rate = pvt->t->src_codec.sample_rate;
  // struct evs_attr *attr = pvt->explicit_dst ? ast_format_get_attribute_data(pvt->explicit_dst) : NULL;
  int rc;

  memset(enc, 0, sizeof(*enc));
  rc = wrap_vocal_evs_init_coder(&enc->enc, EVS_BW_WB, EVS_SF_16K, EVS_BR_5k90, true);
  if (rc <= 0) {
    ast_log(LOG_ERROR, "Error creating the Vocal EVS encoder\n");
    return -1;
  }
  enc->chunk = rc;
  ast_debug(3, "Created encoder (Vocal EVS) with sample rate %d\n", sample_rate);
  return 0;
}

static void evs_enc_destroy(struct ast_trans_pvt *pvt) {
  struct evs_enc_pvt *enc = pvt->pvt;
  wrap_vocal_evs_close_coder(&enc->enc);
  ast_debug(3, "Destroyed encoder (Vocal EVS)\n");
}

static int evs_dec_new(struct ast_trans_pvt *pvt) {
  struct evs_dec_pvt *dec = pvt->pvt;
  const unsigned int sample_rate = pvt->t->src_codec.sample_rate;
  int rc;

  memset(dec, 0, sizeof(*dec));
  rc = wrap_vocal_evs_init_decoder(&dec->dec, EVS_SF_16K);
  if (rc <= 0) {
    ast_log(LOG_ERROR, "Error creating the Vocal EVS decoder\n");
    return -1;
  }
  dec->chunk = rc;
  ast_debug(3, "Created decoder (Vocal EVS) with sample rate %d\n", sample_rate);
  return 0;
}

static void evs_dec_destroy(struct ast_trans_pvt *pvt) {
  struct evs_dec_pvt *dec = pvt->pvt;
  wrap_vocal_evs_close_decoder(&dec->dec);
  ast_debug(3, "Destroyed encoder (Vocal EVS)\n");
}

/* Encoder */

static int lintoevs_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct evs_enc_pvt *enc = pvt->pvt;

	/* XXX We should look at how old the rest of our stream is, and if it
	   is too old, then we should overwrite it entirely, otherwise we can
	   get artifacts of earlier talk that do not belong */
	if (pvt->samples + f->samples > BUFFER_SAMPLES) {
		ast_log(LOG_ERROR, "Out of buffer space\n");
		return -1;
	}
	memcpy(enc->buf + pvt->samples, f->data.ptr, f->datalen);
	pvt->samples += f->samples;
	return 0;
}

static struct ast_frame *lintoevs_frameout(struct ast_trans_pvt *pvt)
{
	struct evs_enc_pvt *enc = pvt->pvt;
	struct ast_frame *result = NULL;
	struct ast_frame *last = NULL;
	int samples = 0; /* output samples */
  struct evs_attr *attr = ast_format_get_attribute_data(pvt->f.subclass.format);
  int do_hf_full = attr && attr->ftmp_hf_only.set == 1 && attr->ftmp_hf_only.val == 1;

	while (pvt->samples >= enc->chunk) {
		struct ast_frame *current = 0;
		int rc;
		unsigned char *out = pvt->outbuf.uc;
		unsigned char *cmr = &out[0];
		unsigned char *toc = &out[1];

    if (do_hf_full) {
      out++;
      out++;
      *cmr = 0xa0;
      *toc = 0x00;
    }

		rc = wrap_vocal_evs_process_coder(&enc->enc, out, BUFFER_SAMPLES * 2, enc->buf, enc->chunk);
		if (rc < 0) {
			ast_log(LOG_ERROR, "Error encoding with Vocal EVS encoder\n");
			pvt->samples = 0;
			return NULL;
		}
		samples += enc->chunk;
		pvt->samples -= enc->chunk;

		if (rc > 0){
			int out_br = 0;
      int datalen_in_bytes = rc/8;

			enum COMPACT_T enc_ct = get_comp_type(rc, 0x00, &out_br);
			if (enc_ct != EVS_Primary)
				ast_log(LOG_ERROR, "Error encoding with Vocal EVS encoder, not prim EVS?\n");

      if (do_hf_full) {
        datalen_in_bytes = (rc/8)+2;
			  *toc |= out_br;
      }

			/* set marker after last sid frame */
			if (enc->last_was_silent == 1 && out_br != Primary_2_4kbps_SID) {
				enc->last_was_silent = 0;
				pvt->f.flags |= AST_FRFLAG_WANTS_MARKER;
			} else {
				/* unset, so loop calls don't make it stick */
				pvt->f.flags &= ~AST_FRFLAG_WANTS_MARKER;
			}

			current = ast_trans_frameout(pvt, datalen_in_bytes, enc->chunk);
			// ast_log(LOG_ERROR, "enc %d -> %d %ld %d\n", rc, pvt->datalen, pvt->f.ts, pvt->f.seqno);
		} else {
			struct ast_frame frm = {
				/* impacts rtp_raw_write rtp ts calc if != voice, try NONE, CNG or VOICE instead? */
				.frametype = AST_FRAME_CNG,
				.src = pvt->t->name,
				.subclass = pvt->f.subclass,
			};
			enc->last_was_silent = 1;
			current = ast_frisolate(&frm);
			// ast_log(LOG_ERROR, "enc %d -> %d %ld %d\n", rc, pvt->datalen, pvt->f.ts, pvt->f.seqno);
		}
		// else
		// 	current = ast_trans_frameout(pvt, 0, enc->expected_chunk_size);

		if (!current) {
			continue;
		} else if (last) {
			/* Append frame */
			AST_LIST_NEXT(last, frame_list) = current;
		} else {
			/* Return first frame */
			result = current;
		}
		last = current;
	}

	/* Move the data at the end of the buffer to the front */
	if (samples) {
		memmove(enc->buf, enc->buf + samples, pvt->samples * sizeof(int16_t));
	}

	return result;
}

static int evstolin_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct evs_dec_pvt *dec = pvt->pvt;
	int16_t *dst = pvt->outbuf.i16;
	int rc;
	unsigned char *in = f->data.ptr;
  struct evs_attr *attr = pvt->explicit_src ? ast_format_get_attribute_data(pvt->explicit_src) : NULL;

  int do_hf_full = attr && attr->ftmp_hf_only.set == 1 && attr->ftmp_hf_only.val == 1;

	if (BUFFER_SAMPLES - pvt->samples < dec->chunk) {
		ast_log(LOG_ERROR, "Out of buffer space\n");
		return -1;
	}

	if (f->datalen == 0) {
		rc = wrap_vocal_evs_process_decoder(&dec->dec, dst + pvt->samples, BUFFER_SAMPLES - pvt->samples, 0, 0, true);
		// ast_log(LOG_ERROR, "concealed missing EVS frame with Vocal EVS decoder %d\n", f->seqno);
		// ast_log(LOG_ERROR, "XXX %d -> %d %ld %d %d %ld\n", rc, pvt->datalen, pvt->f.ts, pvt->f.seqno, f->seqno, f->ts);
		if (rc <= 0) {
			ast_log(LOG_ERROR, "Error concealing missing EVS frame with Vocal EVS decoder\n");
			return -1;
		}
	} else {
    unsigned char *actual_pl_ptr = in;
    int exp_pl_len_in_bits = f->datalen*8;

    if (!do_hf_full) {
      int out_br = 0;
      enum COMPACT_T enc_ct = get_comp_type(exp_pl_len_in_bits, in[0], &out_br);
      if (enc_ct != EVS_Primary) // INVALID_COMPACT_MODE -> hf
        do_hf_full = 1;
    }

    if (do_hf_full) {
      unsigned int toc_byte = in[0];
      int toc_byte_idx = 0;

      if (toc_byte & 0x80) {
        // pr_T_D(toc_byte);
        toc_byte_idx++;
      }
      toc_byte = in[toc_byte_idx];
      // pr_TOC(toc_byte);

      int actual_rx_data_len_in_bits = (f->datalen-(toc_byte_idx+1))*8;


      exp_pl_len_in_bits = lookup_fh_sizes_evs(toc_byte & 0x0f);
      if (actual_rx_data_len_in_bits !=  exp_pl_len_in_bits)
        ast_log(LOG_ERROR, "got f %d %d -> %d %d ?\n", f->datalen, f->samples, actual_rx_data_len_in_bits, exp_pl_len_in_bits);

      //  actual_pl_ptr += toc_byte_idx +1;
       actual_pl_ptr = in + toc_byte_idx +1;
    }

		rc = wrap_vocal_evs_process_decoder(&dec->dec, dst + pvt->samples, BUFFER_SAMPLES - pvt->samples, actual_pl_ptr, exp_pl_len_in_bits, false);
		if (rc <= 0) {
			ast_log(LOG_ERROR, "Error decoding with Vocal EVS decoder\n");
			return -1;
		}
		// ast_log(LOG_ERROR, "dec %d -> %d %ld %d\n", rc, pvt->datalen, pvt->f.ts, pvt->f.seqno);
	}

	if (rc != dec->chunk) {
			ast_log(LOG_ERROR, "Error decoding with Vocal EVS decoder, size mismatch! %d\n", rc);
			return -1;
	}
	pvt->samples += rc;
	pvt->datalen += rc * sizeof(int16_t);

	return 0;
}

static struct ast_translator lintoevs = {
	.name = "lintovevs",
	.src_codec = {
		.name = "slin",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.dst_codec = {
		.name = "vevs",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.format = "vevs",
	.newpvt = evs_enc_new,
	.framein = lintoevs_framein,
	.frameout = lintoevs_frameout,
	.destroy = evs_enc_destroy,
	.sample = slin16_sample,
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.desc_size = sizeof (struct evs_enc_pvt ),
	.native_plc = 1
};

static struct ast_translator evstolin = {
	.name = "vevstolin",
	.src_codec = {
		.name = "vevs",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.dst_codec = {
		.name = "slin",
		.type = AST_MEDIA_TYPE_AUDIO,
		.sample_rate = 16000,
	},
	.format = "slin16",
	.newpvt = evs_dec_new,
	.framein = evstolin_framein,
	.destroy = evs_dec_destroy,
	.sample = vevs_sample,
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.desc_size = sizeof (struct evs_dec_pvt ),
	.native_plc = 1
};

static int unload_module(void)
{
	int res;
  res = ast_rtp_engine_unload_format(ast_format_vevs);
	res |= ast_unregister_translator(&lintoevs);
	res |= ast_unregister_translator(&evstolin);
  maybe_unload_codeclib();

	return res;
}

static int load_module(void)
{
	int res;
  maybe_load_codeclib();
	res = ast_register_translator(&evstolin);
	res |= ast_register_translator(&lintoevs);
  res |= ast_rtp_engine_load_format(ast_format_vevs);

	if (res) {
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Vocal EVS Coder/Decoder",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
);
