// /*** MODULEINFO
// 	<support_level>core</support_level>
//  ***/

#include "asterisk.h"

#include <ctype.h>                      /* for tolower */

#include "asterisk/config.h"
#include "asterisk/codec.h"
#include "asterisk/format_cache.h"
#include "asterisk/module.h"
#include "asterisk/format.h"
#include "asterisk/strings.h"           /* for ast_str_append */
#include "asterisk/utils.h"             /* for MAX, MIN */

#include <stdio.h>
#include <string.h>

enum ftmp_br {
  DEF_ftmp_br5_9,
  DEF_ftmp_br7_2,
  DEF_ftmp_br8,
  DEF_ftmp_br9_6,
  DEF_ftmp_br13_2,
  DEF_ftmp_br16_4,
  DEF_ftmp_br24_4,
  DEF_ftmp_br32,
  DEF_ftmp_br48,
  DEF_ftmp_br64,
  DEF_ftmp_br96,
  DEF_ftmp_br128
};

static const char *get_ftmp_br_str(enum ftmp_br e) {
  switch (e) {
  case DEF_ftmp_br5_9:
    return "5.9";
  case DEF_ftmp_br7_2:
    return "7.2";
  case DEF_ftmp_br8:
    return "8";
  case DEF_ftmp_br9_6:
    return "9.6";
  case DEF_ftmp_br13_2:
    return "13.2";
  case DEF_ftmp_br16_4:
    return "16.4";
  case DEF_ftmp_br24_4:
    return "24.4";
  case DEF_ftmp_br32:
    return "32";
  case DEF_ftmp_br48:
    return "48";
  case DEF_ftmp_br64:
    return "64";
  case DEF_ftmp_br96:
    return "96";
  case DEF_ftmp_br128:
    return "128";
  default:
    return 0;
  }
}

static enum ftmp_br ftmp_br_parse(char *in) {
  if (0 == strcmp(in, "5.9"))
    return DEF_ftmp_br5_9;
  else if (0 == strcmp(in, "7.2"))
    return DEF_ftmp_br7_2;
  else if (0 == strcmp(in, "8"))
    return DEF_ftmp_br8;
  else if (0 == strcmp(in, "9.6"))
    return DEF_ftmp_br9_6;
  else if (0 == strcmp(in, "13.2"))
    return DEF_ftmp_br13_2;
  else if (0 == strcmp(in, "16.4"))
    return DEF_ftmp_br16_4;
  else if (0 == strcmp(in, "24.4"))
    return DEF_ftmp_br24_4;
  else if (0 == strcmp(in, "32"))
    return DEF_ftmp_br32;
  else if (0 == strcmp(in, "48"))
    return DEF_ftmp_br48;
  else if (0 == strcmp(in, "64"))
    return DEF_ftmp_br64;
  else if (0 == strcmp(in, "96"))
    return DEF_ftmp_br96;
  else if (0 == strcmp(in, "128"))
    return DEF_ftmp_br128;
  else
    return 0;
}

enum ftmp_bw {
  DEF_ftmp_bwnb,
  DEF_ftmp_bwwb,
  DEF_ftmp_bwswb,
  DEF_ftmp_bwfb,
  DEF_ftmp_bwnb_wb,
  DEF_ftmp_bwnb_swb,
  DEF_ftmp_bwnb_fb
};

static const char *get_ftmp_bw_str(enum ftmp_bw e) {
  switch (e) {
  case DEF_ftmp_bwnb:
    return "nb";
  case DEF_ftmp_bwwb:
    return "wb";
  case DEF_ftmp_bwswb:
    return "swb";
  case DEF_ftmp_bwfb:
    return "fb";
  case DEF_ftmp_bwnb_wb:
    return "nb-wb";
  case DEF_ftmp_bwnb_swb:
    return "nb-swb";
  case DEF_ftmp_bwnb_fb:
    return "nb-fb";
  default:
    return 0;
  }
}

static enum ftmp_bw ftmp_bw_parse(char *in) {
  if (0 == strcmp(in, "nb"))
    return DEF_ftmp_bwnb;
  else if (0 == strcmp(in, "wb"))
    return DEF_ftmp_bwwb;
  else if (0 == strcmp(in, "swb"))
    return DEF_ftmp_bwswb;
  else if (0 == strcmp(in, "fb"))
    return DEF_ftmp_bwfb;
  else if (0 == strcmp(in, "nb-wb"))
    return DEF_ftmp_bwnb_wb;
  else if (0 == strcmp(in, "nb-swb"))
    return DEF_ftmp_bwnb_swb;
  else if (0 == strcmp(in, "nb-fb"))
    return DEF_ftmp_bwnb_fb;
  else
    return 0;
}

#include "asterisk/vevs.h"

#ifdef DO_GEN_DBG
#include "ftmp_inc.h"
#define wrap_ast_fprintf(x, ...) fprintf(x, __VA_ARGS__)
#define wrap_ast_str_append(...) fprintf(stderr, __VA_ARGS__)
#else
#define wrap_ast_str_append(...) ast_str_append(str, 0, __VA_ARGS__)
#define wrap_ast_fprintf(...) ast_log(LOG_DEBUG, __VA_ARGS__)
#endif

void print_codec_settings(evs_attr *a);
void print_codec_settings(evs_attr *a) {
  if (a->ftmp_evs_mode_switch.set)
    wrap_ast_fprintf( "ftmp_evs_mode_switch -> %d\n",
            a->ftmp_evs_mode_switch.val);
  if (a->ftmp_hf_only.set)
    wrap_ast_fprintf( "ftmp_hf_only -> %d\n", a->ftmp_hf_only.val);
  if (a->ftmp_dtx.set)
    wrap_ast_fprintf( "ftmp_dtx -> %d\n", a->ftmp_dtx.val);
  if (a->ftmp_dtx_recv.set)
    wrap_ast_fprintf( "ftmp_dtx_recv -> %d\n", a->ftmp_dtx_recv.val);
  if (a->ftmp_channels.set)
    wrap_ast_fprintf( "ftmp_channels -> %d\n", a->ftmp_channels.val);
  if (a->ftmp_cmr.set)
    wrap_ast_fprintf( "ftmp_cmr -> %d\n", a->ftmp_cmr.val);
  if (a->ftmp_ch_send.set)
    wrap_ast_fprintf( "ftmp_ch_send -> %d\n", a->ftmp_ch_send.val);
  if (a->ftmp_ch_recv.set)
    wrap_ast_fprintf( "ftmp_ch_recv -> %d\n", a->ftmp_ch_recv.val);
  if (a->ftmp_ch_aw_recv.set)
    wrap_ast_fprintf( "ftmp_ch_aw_recv -> %d\n", a->ftmp_ch_aw_recv.val);
  if (a->ftmp_max_red.set)
    wrap_ast_fprintf( "ftmp_max_red -> %d\n", a->ftmp_max_red.val);
  if (a->ftmp_range_br.a.set)
    wrap_ast_fprintf( "a ftmp_range_br -> %d\n", a->ftmp_range_br.a.val);
  if (a->ftmp_range_br.b.set)
    wrap_ast_fprintf( "b ftmp_range_br -> %d\n", a->ftmp_range_br.b.val);
  if (a->ftmp_range_br_send.a.set)
    wrap_ast_fprintf( "a ftmp_range_br_send -> %d\n",
            a->ftmp_range_br_send.a.val);
  if (a->ftmp_range_br_send.b.set)
    wrap_ast_fprintf( "b ftmp_range_br_send -> %d\n",
            a->ftmp_range_br_send.b.val);
  if (a->ftmp_range_br_recv.a.set)
    wrap_ast_fprintf( "a ftmp_range_br_recv -> %d\n",
            a->ftmp_range_br_recv.a.val);
  if (a->ftmp_range_br_recv.b.set)
    wrap_ast_fprintf( "b ftmp_range_br_recv -> %d\n",
            a->ftmp_range_br_recv.b.val);
  if (a->ftmp_range_bw.a.set)
    wrap_ast_fprintf( "a ftmp_range_bw -> %d\n", a->ftmp_range_bw.a.val);
  if (a->ftmp_range_bw.b.set)
    wrap_ast_fprintf( "b ftmp_range_bw -> %d\n", a->ftmp_range_bw.b.val);
  if (a->ftmp_range_bw_send.a.set)
    wrap_ast_fprintf( "a ftmp_range_bw_send -> %d\n",
            a->ftmp_range_bw_send.a.val);
  if (a->ftmp_range_bw_send.b.set)
    wrap_ast_fprintf( "b ftmp_range_bw_send -> %d\n",
            a->ftmp_range_bw_send.b.val);
  if (a->ftmp_range_bw_recv.a.set)
    wrap_ast_fprintf( "a ftmp_range_bw_recv -> %d\n",
            a->ftmp_range_bw_recv.a.val);
  if (a->ftmp_range_bw_recv.b.set)
    wrap_ast_fprintf( "b ftmp_range_bw_recv -> %d\n",
            a->ftmp_range_bw_recv.b.val);
}

#ifdef DO_GEN_DBG
int gen_ftmp(evs_attr *codec_att) {
  char *tmp;
  int first = 1;
  if (codec_att->ftmp_evs_mode_switch.set) {
    wrap_ast_str_append("%sevs-mode-switch=%d", first ? "" : ";",
                        codec_att->ftmp_evs_mode_switch.val);
    first = 0;
  }

  if (codec_att->ftmp_hf_only.set) {
    wrap_ast_str_append("%shf-only=%d", first ? "" : ";",
                        codec_att->ftmp_hf_only.val);
    first = 0;
  }

  if (codec_att->ftmp_dtx.set) {
    wrap_ast_str_append("%sdtx=%d", first ? "" : ";", codec_att->ftmp_dtx.val);
    first = 0;
  }

  if (codec_att->ftmp_dtx_recv.set) {
    wrap_ast_str_append("%sdtx-recv=%d", first ? "" : ";",
                        codec_att->ftmp_dtx_recv.val);
    first = 0;
  }

  if (codec_att->ftmp_channels.set) {
    wrap_ast_str_append("%schannels=%d", first ? "" : ";",
                        codec_att->ftmp_channels.val);
    first = 0;
  }

  if (codec_att->ftmp_cmr.set) {
    wrap_ast_str_append("%scmr=%d", first ? "" : ";", codec_att->ftmp_cmr.val);
    first = 0;
  }

  if (codec_att->ftmp_range_br.b.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br.a.val);
    const char *b = get_ftmp_br_str(codec_att->ftmp_range_br.b.val);
    wrap_ast_str_append("%sbr=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_br.a.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br.a.val);
    wrap_ast_str_append("%sbr=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_br_send.b.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_send.a.val);
    const char *b = get_ftmp_br_str(codec_att->ftmp_range_br_send.b.val);
    wrap_ast_str_append("%sbr-send=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_br_send.a.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_send.a.val);
    wrap_ast_str_append("%sbr-send=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_br_recv.b.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_recv.a.val);
    const char *b = get_ftmp_br_str(codec_att->ftmp_range_br_recv.b.val);
    wrap_ast_str_append("%sbr-recv=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_br_recv.a.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_recv.a.val);
    wrap_ast_str_append("%sbr-recv=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_bw.b.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw.a.val);
    const char *b = get_ftmp_bw_str(codec_att->ftmp_range_bw.b.val);
    wrap_ast_str_append("%sbw=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_bw.a.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw.a.val);
    wrap_ast_str_append("%sbw=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_bw_send.b.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_send.a.val);
    const char *b = get_ftmp_bw_str(codec_att->ftmp_range_bw_send.b.val);
    wrap_ast_str_append("%sbw-send=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_bw_send.a.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_send.a.val);
    wrap_ast_str_append("%sbw-send=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_bw_recv.b.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_recv.a.val);
    const char *b = get_ftmp_bw_str(codec_att->ftmp_range_bw_recv.b.val);
    wrap_ast_str_append("%sbw-recv=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_bw_recv.a.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_recv.a.val);
    wrap_ast_str_append("%sbw-recv=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_ch_send.set) {
    wrap_ast_str_append("%sch-send=%d", first ? "" : ";",
                        codec_att->ftmp_ch_send.val);
    first = 0;
  }

  if (codec_att->ftmp_ch_recv.set) {
    wrap_ast_str_append("%sch-recv=%d", first ? "" : ";",
                        codec_att->ftmp_ch_recv.val);
    first = 0;
  }

  if (codec_att->ftmp_ch_aw_recv.set) {
    wrap_ast_str_append("%sch-aw-recv=%d", first ? "" : ";",
                        codec_att->ftmp_ch_aw_recv.val);
    first = 0;
  }

  if (codec_att->ftmp_max_red.set) {
    wrap_ast_str_append("%smax-red=%d", first ? "" : ";",
                        codec_att->ftmp_max_red.val);
    first = 0;
  }

  return 0;
}

int parse_ftmp(char *attribs) {
  char *tmp;
  evs_attr *codec_att = &a;
  char stmp1[8];
  char stmp2[8];

  if ((tmp = strstr(attribs, "evs-mode-switch="))) {
    int val = 0;
    if (sscanf(tmp, "evs-mode-switch=%d", &val) > 0) {
      codec_att->ftmp_evs_mode_switch.val = val;
      codec_att->ftmp_evs_mode_switch.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "hf-only="))) {
    int val = 0;
    if (sscanf(tmp, "hf-only=%d", &val) > 0) {
      codec_att->ftmp_hf_only.val = val;
      codec_att->ftmp_hf_only.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "dtx="))) {
    int val = 0;
    if (sscanf(tmp, "dtx=%d", &val) > 0) {
      codec_att->ftmp_dtx.val = val;
      codec_att->ftmp_dtx.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "dtx-recv="))) {
    int val = 0;
    if (sscanf(tmp, "dtx-recv=%d", &val) > 0) {
      codec_att->ftmp_dtx_recv.val = val;
      codec_att->ftmp_dtx_recv.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "channels="))) {
    int val = 0;
    if (sscanf(tmp, "channels=%d", &val) > 0) {
      codec_att->ftmp_channels.val = val;
      codec_att->ftmp_channels.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "cmr="))) {
    int val = 0;
    if (sscanf(tmp, "cmr=%d", &val) > 0) {
      codec_att->ftmp_cmr.val = val;
      codec_att->ftmp_cmr.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "br="))) {
    int scanned = sscanf(tmp, "br=%4[^-]-%4[^-]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_br.a.val = ftmp_br_parse(stmp1);
      codec_att->ftmp_range_br.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_br.b.val = ftmp_br_parse(stmp2);
      codec_att->ftmp_range_br.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "br-send="))) {
    int scanned = sscanf(tmp, "br-send=%4[^-]-%4[^-]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_br_send.a.val = ftmp_br_parse(stmp1);
      codec_att->ftmp_range_br_send.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_br_send.b.val = ftmp_br_parse(stmp2);
      codec_att->ftmp_range_br_send.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "br-recv="))) {
    int scanned = sscanf(tmp, "br-recv=%4[^-]-%4[^-]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_br_recv.a.val = ftmp_br_parse(stmp1);
      codec_att->ftmp_range_br_recv.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_br_recv.b.val = ftmp_br_parse(stmp2);
      codec_att->ftmp_range_br_recv.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "bw="))) {
    int scanned = sscanf(tmp, "bw=%4[^-]-%4[^-]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_bw.a.val = ftmp_bw_parse(stmp1);
      codec_att->ftmp_range_bw.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_bw.b.val = ftmp_bw_parse(stmp2);
      codec_att->ftmp_range_bw.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "bw-send="))) {
    int scanned = sscanf(tmp, "bw-send=%4[^-]-%4[^-]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_bw_send.a.val = ftmp_bw_parse(stmp1);
      codec_att->ftmp_range_bw_send.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_bw_send.b.val = ftmp_bw_parse(stmp2);
      codec_att->ftmp_range_bw_send.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "bw-recv="))) {
    int scanned = sscanf(tmp, "bw-recv=%4[^-]-%4[^-]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_bw_recv.a.val = ftmp_bw_parse(stmp1);
      codec_att->ftmp_range_bw_recv.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_bw_recv.b.val = ftmp_bw_parse(stmp2);
      codec_att->ftmp_range_bw_recv.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "ch-send="))) {
    int val = 0;
    if (sscanf(tmp, "ch-send=%d", &val) > 0) {
      codec_att->ftmp_ch_send.val = val;
      codec_att->ftmp_ch_send.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "ch-recv="))) {
    int val = 0;
    if (sscanf(tmp, "ch-recv=%d", &val) > 0) {
      codec_att->ftmp_ch_recv.val = val;
      codec_att->ftmp_ch_recv.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "ch-aw-recv="))) {
    int val = 0;
    if (sscanf(tmp, "ch-aw-recv=%d", &val) > 0) {
      codec_att->ftmp_ch_aw_recv.val = val;
      codec_att->ftmp_ch_aw_recv.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "max-red="))) {
    int val = 0;
    if (sscanf(tmp, "max-red=%d", &val) > 0) {
      codec_att->ftmp_max_red.val = val;
      codec_att->ftmp_max_red.set = 1;
    }
  }

  print_codec_settings(codec_att);
  gen_ftmp(codec_att);
  return 0;
}
#else

static void ast_gen_ftmp(evs_attr *codec_att, struct ast_str **str) {
  int first = 1;
  if (codec_att->ftmp_evs_mode_switch.set) {
    wrap_ast_str_append("%sevs-mode-switch=%d", first ? "" : ";",
                        codec_att->ftmp_evs_mode_switch.val);
    first = 0;
  }

  if (codec_att->ftmp_hf_only.set) {
    wrap_ast_str_append("%shf-only=%d", first ? "" : ";",
                        codec_att->ftmp_hf_only.val);
    first = 0;
  }

  if (codec_att->ftmp_dtx.set) {
    wrap_ast_str_append("%sdtx=%d", first ? "" : ";", codec_att->ftmp_dtx.val);
    first = 0;
  }

  if (codec_att->ftmp_dtx_recv.set) {
    wrap_ast_str_append("%sdtx-recv=%d", first ? "" : ";",
                        codec_att->ftmp_dtx_recv.val);
    first = 0;
  }

  if (codec_att->ftmp_channels.set) {
    wrap_ast_str_append("%schannels=%d", first ? "" : ";",
                        codec_att->ftmp_channels.val);
    first = 0;
  }

  if (codec_att->ftmp_cmr.set) {
    wrap_ast_str_append("%scmr=%d", first ? "" : ";", codec_att->ftmp_cmr.val);
    first = 0;
  }

  if (codec_att->ftmp_range_br.b.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br.a.val);
    const char *b = get_ftmp_br_str(codec_att->ftmp_range_br.b.val);
    wrap_ast_str_append("%sbr=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_br.a.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br.a.val);
    wrap_ast_str_append("%sbr=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_br_send.b.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_send.a.val);
    const char *b = get_ftmp_br_str(codec_att->ftmp_range_br_send.b.val);
    wrap_ast_str_append("%sbr-send=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_br_send.a.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_send.a.val);
    wrap_ast_str_append("%sbr-send=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_br_recv.b.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_recv.a.val);
    const char *b = get_ftmp_br_str(codec_att->ftmp_range_br_recv.b.val);
    wrap_ast_str_append("%sbr-recv=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_br_recv.a.set) {
    const char *a = get_ftmp_br_str(codec_att->ftmp_range_br_recv.a.val);
    wrap_ast_str_append("%sbr-recv=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_bw.b.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw.a.val);
    const char *b = get_ftmp_bw_str(codec_att->ftmp_range_bw.b.val);
    wrap_ast_str_append("%sbw=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_bw.a.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw.a.val);
    wrap_ast_str_append("%sbw=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_bw_send.b.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_send.a.val);
    const char *b = get_ftmp_bw_str(codec_att->ftmp_range_bw_send.b.val);
    wrap_ast_str_append("%sbw-send=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_bw_send.a.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_send.a.val);
    wrap_ast_str_append("%sbw-send=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_range_bw_recv.b.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_recv.a.val);
    const char *b = get_ftmp_bw_str(codec_att->ftmp_range_bw_recv.b.val);
    wrap_ast_str_append("%sbw-recv=%s-%s", first ? "" : ";", a, b);
    first = 0;
  } else if (codec_att->ftmp_range_bw_recv.a.set) {
    const char *a = get_ftmp_bw_str(codec_att->ftmp_range_bw_recv.a.val);
    wrap_ast_str_append("%sbw-recv=%s", first ? "" : ";", a);
    first = 0;
  }

  if (codec_att->ftmp_ch_send.set) {
    wrap_ast_str_append("%sch-send=%d", first ? "" : ";",
                        codec_att->ftmp_ch_send.val);
    first = 0;
  }

  if (codec_att->ftmp_ch_recv.set) {
    wrap_ast_str_append("%sch-recv=%d", first ? "" : ";",
                        codec_att->ftmp_ch_recv.val);
    first = 0;
  }

  if (codec_att->ftmp_ch_aw_recv.set) {
    wrap_ast_str_append("%sch-aw-recv=%d", first ? "" : ";",
                        codec_att->ftmp_ch_aw_recv.val);
    first = 0;
  }

  if (codec_att->ftmp_max_red.set) {
    wrap_ast_str_append("%smax-red=%d", first ? "" : ";",
                        codec_att->ftmp_max_red.val);
    first = 0;
  }
}

static void ast_parse_ftmp(char *attribs, evs_attr *codec_att) {
  char *tmp;
  char stmp1[8];
  char stmp2[8];

  if ((tmp = strstr(attribs, "evs-mode-switch="))) {
    int val = 0;
    if (sscanf(tmp, "evs-mode-switch=%d", &val) > 0) {
      codec_att->ftmp_evs_mode_switch.val = val;
      codec_att->ftmp_evs_mode_switch.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "hf-only="))) {
    int val = 0;
    if (sscanf(tmp, "hf-only=%d", &val) > 0) {
      codec_att->ftmp_hf_only.val = val;
      codec_att->ftmp_hf_only.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "dtx="))) {
    int val = 0;
    if (sscanf(tmp, "dtx=%d", &val) > 0) {
      codec_att->ftmp_dtx.val = val;
      codec_att->ftmp_dtx.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "dtx-recv="))) {
    int val = 0;
    if (sscanf(tmp, "dtx-recv=%d", &val) > 0) {
      codec_att->ftmp_dtx_recv.val = val;
      codec_att->ftmp_dtx_recv.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "channels="))) {
    int val = 0;
    if (sscanf(tmp, "channels=%d", &val) > 0) {
      codec_att->ftmp_channels.val = val;
      codec_att->ftmp_channels.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "cmr="))) {
    int val = 0;
    if (sscanf(tmp, "cmr=%d", &val) > 0) {
      codec_att->ftmp_cmr.val = val;
      codec_att->ftmp_cmr.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "br="))) {
    int scanned = sscanf(tmp, "br=%4[^-;]-%4[^-;]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_br.a.val = ftmp_br_parse(stmp1);
      codec_att->ftmp_range_br.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_br.b.val = ftmp_br_parse(stmp2);
      codec_att->ftmp_range_br.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "br-send="))) {
    int scanned = sscanf(tmp, "br-send=%4[^-;]-%4[^-;]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_br_send.a.val = ftmp_br_parse(stmp1);
      codec_att->ftmp_range_br_send.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_br_send.b.val = ftmp_br_parse(stmp2);
      codec_att->ftmp_range_br_send.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "br-recv="))) {
    int scanned = sscanf(tmp, "br-recv=%4[^-;]-%4[^-;]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_br_recv.a.val = ftmp_br_parse(stmp1);
      codec_att->ftmp_range_br_recv.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_br_recv.b.val = ftmp_br_parse(stmp2);
      codec_att->ftmp_range_br_recv.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "bw="))) {
    int scanned = sscanf(tmp, "bw=%4[^-;]-%4[^-;]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_bw.a.val = ftmp_bw_parse(stmp1);
      codec_att->ftmp_range_bw.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_bw.b.val = ftmp_bw_parse(stmp2);
      codec_att->ftmp_range_bw.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "bw-send="))) {
    int scanned = sscanf(tmp, "bw-send=%4[^-;]-%4[^-;]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_bw_send.a.val = ftmp_bw_parse(stmp1);
      codec_att->ftmp_range_bw_send.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_bw_send.b.val = ftmp_bw_parse(stmp2);
      codec_att->ftmp_range_bw_send.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "bw-recv="))) {
    int scanned = sscanf(tmp, "bw-recv=%4[^-;]-%4[^-;]", stmp1, stmp2);
    if (scanned > 0) {
      codec_att->ftmp_range_bw_recv.a.val = ftmp_bw_parse(stmp1);
      codec_att->ftmp_range_bw_recv.a.set = 1;
    }
    if (scanned > 1) {
      codec_att->ftmp_range_bw_recv.b.val = ftmp_bw_parse(stmp2);
      codec_att->ftmp_range_bw_recv.b.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "ch-send="))) {
    int val = 0;
    if (sscanf(tmp, "ch-send=%d", &val) > 0) {
      codec_att->ftmp_ch_send.val = val;
      codec_att->ftmp_ch_send.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "ch-recv="))) {
    int val = 0;
    if (sscanf(tmp, "ch-recv=%d", &val) > 0) {
      codec_att->ftmp_ch_recv.val = val;
      codec_att->ftmp_ch_recv.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "ch-aw-recv="))) {
    int val = 0;
    if (sscanf(tmp, "ch-aw-recv=%d", &val) > 0) {
      codec_att->ftmp_ch_aw_recv.val = val;
      codec_att->ftmp_ch_aw_recv.set = 1;
    }
  }

  if ((tmp = strstr(attribs, "max-red="))) {
    int val = 0;
    if (sscanf(tmp, "max-red=%d", &val) > 0) {
      codec_att->ftmp_max_red.val = val;
      codec_att->ftmp_max_red.set = 1;
    }
  }
}
#endif


#define _AT_CMP_RANGE(td,tta, ttb, which)  \
if (tta.which.set && ttb.which.set) { \
	td.which.set = 1; \
	td.which.val = MIN(tta.which.val, ttb.which.val); \
} else if (tta.which.set) { \
	td.which.set = 1; \
	td.which.val = tta.which.val; \
} else if (ttb.which.set) { \
	td.which.set = 1; \
	td.which.val = ttb.which.val; \
} else { \
	td.which.set = 0; \
}
#define _FIX_RANGE(dest) \
  if (dest.a.val == dest.b.val && dest.a.set == dest.b.set) \
      dest.b.set = 0;

#define AT_CMP_RANGE(dst, src1, src2) _AT_CMP_RANGE(dst, src1, src2, a) _AT_CMP_RANGE(dst, src1, src2, b) _FIX_RANGE(dst)

#define SET_E_AT(xx,yy) .xx.set =1, .xx.val = yy,
#define SET_E_ATR1(xx,yy) SET_E_AT(xx.a,yy)
#define SET_E_ATR2(xx,yy, zz) SET_E_AT(xx.a,yy) SET_E_AT(xx.b,zz)

static evs_attr default_attrib = {
	// SET_E_AT(ftmp_hf_only, 1)
	SET_E_ATR1(ftmp_range_br, DEF_ftmp_br5_9)
	SET_E_ATR2(ftmp_range_bw, DEF_ftmp_bwnb, DEF_ftmp_bwwb)
	SET_E_AT(ftmp_dtx, 1)
	SET_E_AT(ftmp_max_red, 0)
};

static void evs_destroy(struct ast_format *format)
{
	struct evs_attr *attr = ast_format_get_attribute_data(format);

	ast_free(attr);
}

static void attr_init(struct evs_attr *attr)
{
	// memset(attr, 0, sizeof(*attr));
	*attr = default_attrib;
}

static int evs_clone(const struct ast_format *src, struct ast_format *dst)
{
	struct evs_attr *original = ast_format_get_attribute_data(src);
	struct evs_attr *attr = ast_malloc(sizeof(*attr));

	if (!attr) {
		return -1;
	}

	if (original) {
		*attr = *original;
	} else {
		attr_init(attr);
	}

	ast_format_set_attribute_data(dst, attr);
  wrap_ast_fprintf( "ftmp cloned\n");
  print_codec_settings(attr);

	return 0;
}

static struct ast_format *evs_parse_sdp_fmtp(const struct ast_format *format, const char *attributes)
{
	char *attribs = ast_strdupa(attributes), *attrib;
	struct ast_format *cloned;
	struct evs_attr *codec_att;

	cloned = ast_format_clone(format);
	if (!cloned) {
		return NULL;
	}
	codec_att = ast_format_get_attribute_data(cloned);

	/* lower-case everything, so we are case-insensitive */
	for (attrib = attribs; *attrib; ++attrib) {
		*attrib = tolower(*attrib);
	} /* based on channels/chan_sip.c:process_a_sdp_image() */

	ast_parse_ftmp(attribs, codec_att);
  wrap_ast_fprintf( "ftmp parse\n");
	print_codec_settings(codec_att);
	return cloned;
}

static void evs_generate_sdp_fmtp(const struct ast_format *format, unsigned int payload, struct ast_str **str)
{
	struct evs_attr *attr = ast_format_get_attribute_data(format);

	if (!attr) {
		attr = &default_attrib;
	}

	ast_str_append(str, 0, "a=fmtp:%d ", payload);
	ast_gen_ftmp(attr, str);
	ast_str_append(str, 0, "\r\n");
  wrap_ast_fprintf( "ftmp gen\n");
	print_codec_settings(attr);


	// ast_str_append(str, 0, "a=fmtp:%u hf-only=1;br=5.9;bw=nb-wb;max-red=0;dtx=0\r\n", payload);
}

static enum ast_format_cmp_res evs_cmp(const struct ast_format *format1, const struct ast_format *format2)
{
  struct evs_attr *attr1 = ast_format_get_attribute_data(format1);
	struct evs_attr *attr2 = ast_format_get_attribute_data(format2);

  // wrap_ast_fprintf( "CMP %s %s -> %p %p\n", ast_format_get_name(format1), ast_format_get_name(format2), attr1, attr2);

	if (!attr1) {
		attr1 = &default_attrib;
	}

	if (!attr2) {
		attr2 = &default_attrib;
	}

	if (ast_format_get_sample_rate(format1) == ast_format_get_sample_rate(format2)) {
		return AST_FORMAT_CMP_EQUAL;
	}

	return AST_FORMAT_CMP_NOT_EQUAL;
}

static struct ast_format *evs_getjoint(const struct ast_format *format1, const struct ast_format *format2)
{
	struct evs_attr *attr1 = ast_format_get_attribute_data(format1);
	struct evs_attr *attr2 = ast_format_get_attribute_data(format2);
	struct evs_attr *attr_res;
	struct ast_format *jointformat = NULL;

	if (!attr1) {
		attr1 = &default_attrib;
	}

	if (!attr2) {
		attr2 = &default_attrib;
	}

  jointformat = ast_format_clone(format1);

	attr_res = ast_format_get_attribute_data(jointformat);

  attr_res->ftmp_hf_only.val = MAX(attr1->ftmp_hf_only.val,attr2->ftmp_hf_only.val);
  attr_res->ftmp_hf_only.set = MAX(attr1->ftmp_hf_only.set,attr2->ftmp_hf_only.set);

  attr_res->ftmp_dtx.val = MAX(attr1->ftmp_dtx.val,attr2->ftmp_dtx.val);
  attr_res->ftmp_dtx.set = MAX(attr1->ftmp_dtx.set,attr2->ftmp_dtx.set);

  AT_CMP_RANGE(attr_res->ftmp_range_br, attr1->ftmp_range_br, attr2->ftmp_range_br)
  AT_CMP_RANGE(attr_res->ftmp_range_bw, attr1->ftmp_range_bw, attr2->ftmp_range_bw)
  /* Limit bitrate to 5.9. */
  attr_res->ftmp_range_br.a.set = 1;
  attr_res->ftmp_range_br.a.val = DEF_ftmp_br5_9;
  attr_res->ftmp_range_br.b.set = 0;

  attr_res->ftmp_max_red.val = MIN(attr1->ftmp_max_red.val,attr2->ftmp_max_red.val);
  attr_res->ftmp_max_red.set = MAX(attr1->ftmp_max_red.set,attr2->ftmp_max_red.set);


  wrap_ast_fprintf( "ftmp joint\n");
  print_codec_settings(attr_res);
  wrap_ast_fprintf( "ftmp 1\n");
  print_codec_settings(attr1);
  wrap_ast_fprintf( "ftmp 2\n");
  print_codec_settings(attr2);
	return jointformat;
}
/* pointless unused functions ?
static struct ast_format *evs_set(const struct ast_format *format, const char *name, const char *value)
{
  struct ast_format *cloned;
	struct evs_attr *attr;
	int val;

	if (!(cloned = ast_format_clone(format))) {
		return NULL;
	}
  attr = ast_format_get_attribute_data(cloned);

	if (!strcasecmp(name, CODEC_VEVS_ATTR_HF_ONLY)) {
    if (sscanf(value, "%d", &val) != 1) {
      ast_log(LOG_ERROR, "error parsing codec attr %s -> %s\n", name, value);
      ao2_ref(cloned, -1);
      return NULL;
    }
    attr->ftmp_hf_only.val = val;
    attr->ftmp_hf_only.set = val ? 1 : 0;
  }

  ast_log(LOG_ERROR, "set attribute type %s -> %s\n", name, value);
	return cloned;
}

static const void *evs_get(const struct ast_format *format, const char *name)
{
	struct evs_attr *attr = ast_format_get_attribute_data(format);
	int *val = NULL;

	if (!attr) {
		return NULL;
	}

	if (!strcasecmp(name, CODEC_VEVS_ATTR_HF_ONLY)) {
    if (attr->ftmp_hf_only.set)
		  return &attr->ftmp_hf_only.val;
  }
  return val;
}
*/
static int evs_load_cfg(void)
{
	struct ast_variable *var;
	struct ast_flags config_flags = { 0 };
	struct ast_config *cfg = ast_config_load("codecs.conf", config_flags);

	if (cfg == CONFIG_STATUS_FILEMISSING || cfg == CONFIG_STATUS_FILEUNCHANGED || cfg == CONFIG_STATUS_FILEINVALID) {
		return 0;
	}

	for (var = ast_variable_browse(cfg, "vevs"); var; var = var->next) {
		if (!strcasecmp(var->name, CODEC_VEVS_ATTR_HF_ONLY)) {
      int val = atoi(var->value);
			if (val >= 0 && val <= 1) {
        default_attrib.ftmp_hf_only.val = val;
        default_attrib.ftmp_hf_only.set = val ? 1 : 0;
      }
		}
	}
	ast_config_destroy(cfg);
	return 0;
}

static struct ast_format_interface evs_interface = {
	.format_destroy = evs_destroy,
	.format_clone = evs_clone,
	.format_cmp = evs_cmp,
	.format_get_joint = evs_getjoint,
/* pointless unused functions ?
  .format_attribute_set = evs_set,
  .format_attribute_get = evs_get,
*/
	.format_parse_sdp_fmtp = evs_parse_sdp_fmtp,
	.format_generate_sdp_fmtp = evs_generate_sdp_fmtp,
};

static int load_module(void)
{
	if (ast_format_interface_register("vevs", &evs_interface)) {
		return AST_MODULE_LOAD_DECLINE;
	}

  evs_load_cfg();

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS | AST_MODFLAG_LOAD_ORDER, "Vocal EVS Format Attribute Module",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_REALTIME_DRIVER,
);
