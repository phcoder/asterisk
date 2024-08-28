#pragma once

struct maybe_set {
  int set;
  int val;
};
typedef struct maybe_set maybe_set;

struct maybe_set_twice {
  struct maybe_set a;
  struct maybe_set b;
};
typedef struct maybe_set_twice maybe_set_twice;

struct evs_attr {
  struct maybe_set ftmp_evs_mode_switch;
  struct maybe_set ftmp_hf_only;
  struct maybe_set ftmp_dtx;
  struct maybe_set ftmp_dtx_recv;
  struct maybe_set ftmp_channels;
  struct maybe_set ftmp_cmr;
  struct maybe_set ftmp_ch_send;
  struct maybe_set ftmp_ch_recv;
  struct maybe_set ftmp_ch_aw_recv;
  struct maybe_set ftmp_max_red;
  struct maybe_set_twice ftmp_range_br;
  struct maybe_set_twice ftmp_range_br_send;
  struct maybe_set_twice ftmp_range_br_recv;
  struct maybe_set_twice ftmp_range_bw;
  struct maybe_set_twice ftmp_range_bw_send;
  struct maybe_set_twice ftmp_range_bw_recv;
};
typedef struct evs_attr evs_attr;

#define CODEC_VEVS_ATTR_HF_ONLY "hf_only"


// ##
//  minimum set of defines to make mockup happy
#define EVS_BW_NB 0u
#define EVS_BW_WB 1u
#define EVS_SF_8K 8000u
#define EVS_SF_16K 16000u
#define EVS_BR_5k90 5900u

#define BUFFER_SAMPLES 5760

#if defined(__x86_64__)
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef signed short sint15;
typedef unsigned int uint32;
typedef signed int sint31;
typedef unsigned long long uint64;
typedef long long sint63;
typedef uint16 vbool;
#else // probably arm
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef signed short sint15;
typedef unsigned long uint32;
typedef signed long sint31;
typedef unsigned long long uint64;
typedef long long sint63;
typedef uint16 vbool;
#endif
// ##

typedef enum {
  MSG_TYPE_ALLOC_E,
  MSG_TYPE_ALLOC_D,
  MSG_TYPE_DEALLOC_E,
  MSG_TYPE_DEALLOC_D,
  MSG_TYPE_PROCESS_E,
  MSG_TYPE_PROCESS_D
} MessageType;

typedef enum { T_ENC, T_DEC } coder_side;

typedef struct {
  MessageType type;
  union {
	struct {
	  uint16 bandwidth;
	  uint16 sampling_frequency;
	  uint32 bitrate;
	  vbool dtx_enable;
	} init_coder;
	struct {
	  uint8 *enc_data;
	  uint16 enc_data_size;
	  const sint15 *enc_samples;
	  uint16 sample_count;
	} process_coder;
	struct {
	} close_coder;
	struct {
	  uint16 sampling_frequency;
	} init_decoder;
	struct {
	  sint15 *dec_samples;
	  uint16 max_sample_count;
	  uint8 *dec_data;
	  uint16 dec_data_size;
	  vbool bad_frame;
	} process_decoder;
	struct {
	} close_decoder;
  } fpars;
  union {
	unsigned char b[2048];
	short s16[2048 / sizeof(short)];
  } data;
  int ret;
} Codec_Message;

#define BUFFER_SIZE 4096
#define ARR_SZ(x) (sizeof(x) / sizeof(x[0]))

#define SOCKET_NAME "\0/tmp/ast_codec.socket"

#define socket_or_die(var, dom, type, prot) \
  var = socket(dom, type, prot);            \
  if (var == -1) {                          \
	perror("socket");                       \
	return EXIT_FAILURE;                    \
  }

#define bind_or_die(sockvar)                                                       \
  struct sockaddr_un name;                                                         \
  memset(&name, 0, sizeof(struct sockaddr_un));                                    \
  name.sun_family = AF_UNIX;                                                       \
  strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);                  \
  ret = bind(sockvar, (const struct sockaddr *)&name, sizeof(struct sockaddr_un)); \
  if (ret == -1) {                                                                 \
	perror("bind");                                                                \
	return EXIT_FAILURE;                                                           \
  }

#define connect_or_die(sockvar)                                                       \
  struct sockaddr_un name;                                                            \
  memset(&name, 0, sizeof(struct sockaddr_un));                                       \
  name.sun_family = AF_UNIX;                                                          \
  strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);                     \
  ret = connect(sockvar, (const struct sockaddr *)&name, sizeof(struct sockaddr_un)); \
  if (ret == -1) {                                                                    \
	perror("connect");                                                                \
	return EXIT_FAILURE;                                                              \
  }

#define listen_or_die(sockvar, qlen) \
  ret = listen(sockvar, qlen);       \
  if (ret == -1) {                   \
	perror("listen");                \
	return EXIT_FAILURE;             \
  }

#define nb_or_die(sockvar)                                                      \
  int nbret = fcntl(sockvar, F_SETFL, fcntl(sockvar, F_GETFL, 0) | O_NONBLOCK); \
  if (nbret == -1) {                                                            \
	perror("nonblock");                                                         \
	return;                                                                     \
  }
#define accept_or_die(var, sockvar, flags)   \
  var = accept4(sockvar, NULL, NULL, flags); \
  if (var == -1) {                           \
	perror("accept");                        \
	return;                                  \
  }

#define read_or_die(...)                 \
  ret = recv(__VA_ARGS__, MSG_NOSIGNAL); \
  if (ret == -1) {                       \
	perror("read");                      \
	return;                              \
  }

