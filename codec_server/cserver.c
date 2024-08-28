/*
* Copyright 2024 sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
* Author: Eric Wild <ewild@sysmocom.de>
*
* SPDX-License-Identifier: 0BSD
*
* Permission to use, copy, modify, and/or distribute this software for any purpose
* with or without fee is hereby granted.THE SOFTWARE IS PROVIDED "AS IS" AND THE
* AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
* CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
* USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>
// #include <sys/queue.h>

#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../include/asterisk/vevs.h"

static_assert(BUFFER_SIZE >= sizeof(Codec_Message), "check msg size!");

#ifndef ast_log
#define ast_log(x, ...) fprintf(stderr, __VA_ARGS__)
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

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
// #############

static volatile int do_exit = 0;

typedef void (*write_cb)(void *c);
typedef void (*read_cb)(void *c);
typedef void (*err_cb)(void *c);

typedef struct {
  Codec_Message m;
  coder_side s;
  void *codec_priv;
  int chunksz;
} codec_state;

typedef struct {
  int top_epoll_fd;
  int fd;
  write_cb wcb;
  read_cb rcb;
  err_cb ecb;
  codec_state cs;
} epoll_ctx;

static epoll_ctx *new_epoll_ctx(int epfd, int fd, read_cb rc, write_cb wc, err_cb ec) {
  epoll_ctx *t = calloc(1, sizeof(epoll_ctx));
  // t->databuf = calloc(1, BUFFER_SIZE);
  t->top_epoll_fd = epfd;
  t->fd = fd;
  t->rcb = rc;
  t->wcb = wc;
  t->ecb = ec;
  return t;
}

static void err_data_cb(void *p) {
  epoll_ctx *c = (epoll_ctx *)p;
  epoll_ctl(c->top_epoll_fd, EPOLL_CTL_DEL, c->fd, 0);
  close(c->fd);

  if (c->cs.codec_priv) {
    vocal_set_data(c->cs.codec_priv);
    switch (c->cs.s) {
      case T_ENC: vocal_evs_close_coder(); break;
      case T_DEC: vocal_evs_close_decoder(); break;
    }
    vocal_destroy_data(c->cs.codec_priv);
    c->cs.codec_priv = 0;
  }

  free(p);
}

static void r_data_cb(void *p) {
  int ret;
  epoll_ctx *c = (epoll_ctx *)p;
  Codec_Message respmsg;

  do {
    ret = recv(c->fd, (void *)&c->cs.m, sizeof(Codec_Message), MSG_NOSIGNAL);
  } while (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
  if (ret == -1) {
    perror("recv");
    err_data_cb(p);
    return;
  }

  // fprintf(stderr, "%s %d\n", __PRETTY_FUNCTION__, ret);

  if (c->cs.m.type == MSG_TYPE_DEALLOC_E || c->cs.m.type == MSG_TYPE_DEALLOC_D) {
    err_data_cb(p);
    return;
  }

  else if (c->cs.m.type == MSG_TYPE_ALLOC_E || c->cs.m.type == MSG_TYPE_ALLOC_D) {
    assert(c->cs.codec_priv == 0);
    c->cs.codec_priv = vocal_allocate_data();
    assert(c->cs.codec_priv != 0);
    vocal_set_data(c->cs.codec_priv);

    if (c->cs.m.type == MSG_TYPE_ALLOC_E) {
      c->cs.chunksz =
          vocal_evs_init_coder(c->cs.m.fpars.init_coder.bandwidth, c->cs.m.fpars.init_coder.sampling_frequency,
                               c->cs.m.fpars.init_coder.bitrate, c->cs.m.fpars.init_coder.dtx_enable);
      c->cs.s = T_ENC;
    } else if (c->cs.m.type == MSG_TYPE_ALLOC_D) {
      c->cs.chunksz = vocal_evs_init_decoder(c->cs.m.fpars.init_decoder.sampling_frequency);
      c->cs.s = T_DEC;
    }
    ret = c->cs.chunksz;
    assert(ret >= 0);
  } else if (c->cs.m.type == MSG_TYPE_PROCESS_E) {
    assert(c->cs.codec_priv != 0);
    vocal_set_data(c->cs.codec_priv);
    // returns BITS
    ret = vocal_evs_process_coder(respmsg.data.b, ARR_SZ(respmsg.data.b), c->cs.m.data.s16,
                                  c->cs.m.fpars.process_coder.sample_count);
    assert(ret >= 0);
  } else if (c->cs.m.type == MSG_TYPE_PROCESS_D) {
    assert(c->cs.codec_priv != 0);
    vocal_set_data(c->cs.codec_priv);
    ret =
        vocal_evs_process_decoder(respmsg.data.s16, ARR_SZ(respmsg.data.s16), c->cs.m.data.b,
                                  c->cs.m.fpars.process_decoder.dec_data_size, c->cs.m.fpars.process_decoder.bad_frame);
    assert(ret == c->cs.chunksz);
  } else {
    fprintf(stderr, "unknown msg %s\n", __PRETTY_FUNCTION__);
    err_data_cb(p);
    exit(1);
  }

  respmsg.type = c->cs.m.type;
  respmsg.ret = ret;
  do {
    ret = send(c->fd, (void *)&respmsg, sizeof(Codec_Message), MSG_NOSIGNAL);
  } while (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
  if (ret == -1) {
    perror("send");
    err_data_cb(p);
    return;
  }
  // shutdown(c->fd, SHUT_RDWR);
}
static void w_data_cb(void *p) { fprintf(stderr, "%s\n", __PRETTY_FUNCTION__); }

static void r_acc_cb(void *p) {
  int client_sock = -1;
  epoll_ctx *c = (epoll_ctx *)p;
  fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);

  // accept_or_die(client_sock, c->fd, O_CLOEXEC | O_NONBLOCK);
  // nb_or_die(client_sock);

  do {
    client_sock = accept4(c->fd, NULL, NULL, O_CLOEXEC | O_NONBLOCK);
  } while (client_sock == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
  if (client_sock == -1) {
    perror("accept");
    return;
  }

  struct epoll_event e = {.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
                          .data.ptr = new_epoll_ctx(c->top_epoll_fd, client_sock, r_data_cb, w_data_cb, err_data_cb)};
  epoll_ctl(c->top_epoll_fd, EPOLL_CTL_ADD, client_sock, &e);
}
static void w_acc_cb(void *p) {
  epoll_ctx *c = (epoll_ctx *)p;
  fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
}
static void err_acc_cb(void *p) {
  epoll_ctx *c = (epoll_ctx *)p;
  fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
}

static void dbg_print_epflags(int flags) {
  if (flags & EPOLLRDHUP)
    fprintf(stderr, "EPOLLRDHUP ");
  if (flags & EPOLLHUP)
    fprintf(stderr, "EPOLLHUP ");
  if (flags & EPOLLERR)
    fprintf(stderr, "EPOLLERR ");
}

// void *server_thread() {
//   int ret;
//   int server_sock;
//   int data_socket;
//   int result;

//   unlink(SOCKET_NAME);
//   socket_or_die(server_sock, AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
//   bind_or_die(server_sock);
//   listen_or_die(server_sock, 20);
//   // nb_or_die(server_sock);

//   int ep_fd = epoll_create1(0);
//   struct epoll_event event = {.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
//                               .data.ptr = new_epoll_ctx(ep_fd, server_sock, r_acc_cb, w_acc_cb, err_acc_cb)};

//   epoll_ctl(ep_fd, EPOLL_CTL_ADD, server_sock, &event);

//   while (!do_exit) {
//     struct epoll_event rdy_ev[10];
//     int ready_cnt = epoll_wait(ep_fd, rdy_ev, 10, 100);
//     for (int i = 0; i < ready_cnt; i++) {
//       epoll_ctx *s = (epoll_ctx *)rdy_ev[i].data.ptr;

//       if (rdy_ev[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
//         dbg_print_epflags(rdy_ev[i].events);
//         fprintf(stderr, "sock err %s\n", __PRETTY_FUNCTION__);
//         rdy_ev[i].events &= (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
//         s->ecb(s);
//       }
//       if (rdy_ev[i].events & EPOLLIN)
//         s->rcb(s);
//       if (rdy_ev[i].events & EPOLLOUT)
//         s->wcb(s);
//     }
//   }

//   close(server_sock);
//   close(ep_fd);
//   unlink(SOCKET_NAME);
//   return EXIT_SUCCESS;
// }

// extern void *client_thread(void);

static void int_h(int nop) { do_exit = 1; }

int main(void) {
  // pthread_t listener, handler;
  // struct stat st;

  // signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, int_h);
  maybe_load_codeclib();

  // pthread_create(&listener, NULL, server_thread, NULL);
  // pthread_create(&handler, NULL, client_thread, NULL);

  {
    int ret;
    int server_sock;
    int data_socket;
    int result;

    unlink(SOCKET_NAME);
    socket_or_die(server_sock, AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    bind_or_die(server_sock);
    listen_or_die(server_sock, 20);

    int ep_fd = epoll_create1(0);
    struct epoll_event event = {.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR,
                                .data.ptr = new_epoll_ctx(ep_fd, server_sock, r_acc_cb, w_acc_cb, err_acc_cb)};

    epoll_ctl(ep_fd, EPOLL_CTL_ADD, server_sock, &event);

    while (!do_exit) {
      struct epoll_event rdy_ev[10];
      int ready_cnt = epoll_wait(ep_fd, rdy_ev, 10, 500);
      for (int i = 0; i < ready_cnt; i++) {
        epoll_ctx *s = (epoll_ctx *)rdy_ev[i].data.ptr;

        if (rdy_ev[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
          dbg_print_epflags(rdy_ev[i].events);
          fprintf(stderr, "sock err %s\n", __PRETTY_FUNCTION__);
          rdy_ev[i].events &= (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
          s->ecb(s);
        }
        if (rdy_ev[i].events & EPOLLIN)
          s->rcb(s);
        if (rdy_ev[i].events & EPOLLOUT)
          s->wcb(s);
      }
    }

    close(ep_fd);
    close(server_sock);
    unlink(SOCKET_NAME);
    return EXIT_SUCCESS;
  }
  // pthread_join(listener, NULL);
  // pthread_join(handler, NULL);

  maybe_unload_codeclib();
  return 0;
}
