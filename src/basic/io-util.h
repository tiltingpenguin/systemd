/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "macro.h"
#include "time-util.h"

int flush_fd(int fd);

ssize_t loop_read(int fd, void *buf, size_t nbytes, bool do_poll);
int loop_read_exact(int fd, void *buf, size_t nbytes, bool do_poll);
int loop_write(int fd, const void *buf, size_t nbytes, bool do_poll);

int pipe_eof(int fd);

int ppoll_usec(struct pollfd *fds, size_t nfds, usec_t timeout);
int fd_wait_for_event(int fd, int event, usec_t timeout);

ssize_t sparse_write(int fd, const void *p, size_t sz, size_t run_length);

static inline size_t IOVEC_TOTAL_SIZE(const struct iovec *i, size_t n) {
        size_t r = 0;

        for (size_t j = 0; j < n; j++)
                r += i[j].iov_len;

        return r;
}

static inline bool IOVEC_INCREMENT(struct iovec *i, size_t n, size_t k) {
        /* Returns true if there is nothing else to send (bytes written cover all of the iovec),
         * false if there's still work to do. */

        for (size_t j = 0; j < n; j++) {
                size_t sub;

                if (i[j].iov_len == 0)
                        continue;
                if (k == 0)
                        return false;

                sub = MIN(i[j].iov_len, k);
                i[j].iov_len -= sub;
                i[j].iov_base = (uint8_t*) i[j].iov_base + sub;
                k -= sub;
        }

        assert(k == 0); /* Anything else would mean that we wrote more bytes than available,
                         * or the kernel reported writing more bytes than sent. */
        return true;
}

static inline bool FILE_SIZE_VALID(uint64_t l) {
        /* ftruncate() and friends take an unsigned file size, but actually cannot deal with file sizes larger than
         * 2^63 since the kernel internally handles it as signed value. This call allows checking for this early. */

        return (l >> 63) == 0;
}

static inline bool FILE_SIZE_VALID_OR_INFINITY(uint64_t l) {

        /* Same as above, but allows one extra value: -1 as indication for infinity. */

        if (l == UINT64_MAX)
                return true;

        return FILE_SIZE_VALID(l);

}

#define IOVEC_NULL (struct iovec) {}
#define IOVEC_MAKE(base, len) (struct iovec) { .iov_base = (base), .iov_len = (len) }
#define IOVEC_MAKE_STRING(string)               \
        ({                                      \
                char *_s = (char*) (string);    \
                IOVEC_MAKE(_s, strlen(_s));     \
        })

char* set_iovec_string_field(struct iovec *iovec, size_t *n_iovec, const char *field, const char *value);
char* set_iovec_string_field_free(struct iovec *iovec, size_t *n_iovec, const char *field, char *value);

struct iovec_wrapper {
        struct iovec *iovec;
        size_t count;
};

struct iovec_wrapper *iovw_new(void);
struct iovec_wrapper *iovw_free(struct iovec_wrapper *iovw);
struct iovec_wrapper *iovw_free_free(struct iovec_wrapper *iovw);

DEFINE_TRIVIAL_CLEANUP_FUNC(struct iovec_wrapper*, iovw_free_free);

void iovw_free_contents(struct iovec_wrapper *iovw, bool free_vectors);

int iovw_put(struct iovec_wrapper *iovw, void *data, size_t len);
static inline int iovw_consume(struct iovec_wrapper *iovw, void *data, size_t len) {
        /* Move data into iovw or free on error */
        int r;

        r = iovw_put(iovw, data, len);
        if (r < 0)
                free(data);

        return r;
}

static inline bool iovw_isempty(const struct iovec_wrapper *iovw) {
        return !iovw || iovw->count == 0;
}

int iovw_put_string_field(struct iovec_wrapper *iovw, const char *field, const char *value);
int iovw_put_string_field_free(struct iovec_wrapper *iovw, const char *field, char *value);
void iovw_rebase(struct iovec_wrapper *iovw, char *old, char *new);
size_t iovw_size(struct iovec_wrapper *iovw);
int iovw_append(struct iovec_wrapper *target, const struct iovec_wrapper *source);

void iovec_array_free(struct iovec *iov, size_t n);
