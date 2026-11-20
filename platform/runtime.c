/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "d_main.h"
#include "m_argv.h"
#include "runtime.h"

#define WAD_METADATA ((const volatile uint32_t *)0x10fffff0u)
#define WAD_MAGIC 0x44415756u
#define SAVE_SLOTS 6
#define SAVE_LIMIT 0x2c000u
#define UART1 ((volatile uint32_t *)0x90030000u)

extern unsigned char __heap_start[];
static uintptr_t heap_break = (uintptr_t)__heap_start;
static uintptr_t heap_limit = (uintptr_t)WAD_METADATA;
static const unsigned char *wad_data;
static size_t wad_size;
static GameMode_t wad_mode;

struct session_file {
    unsigned char *data;
    size_t size;
};

struct open_file {
    const unsigned char *data;
    struct session_file *save;
    size_t size;
    size_t offset;
    int flags;
    int used;
};

static struct session_file saves[SAVE_SLOTS];
static struct session_file config;
static struct open_file files[8];

GameMode_t vflash_wad_mode(void)
{
    return wad_mode;
}

unsigned vflash_heap_available(void)
{
    return heap_limit - heap_break;
}

void _exit(int status)
{
    (void)status;
    for (;;)
        __asm__ volatile("nop");
}

void vflash_start(void)
{
    static char *argv[] = {"doom-vflash", NULL};
    uintptr_t start = WAD_METADATA[1];
    size_t length = WAD_METADATA[2];
    unsigned mode = WAD_METADATA[3];

    setbuf(stdout, NULL);
    if (WAD_METADATA[0] != WAD_MAGIC || start < heap_break ||
        start >= (uintptr_t)WAD_METADATA || length < 12 ||
        length > (uintptr_t)WAD_METADATA - start ||
        mode > retail || memcmp((const void *)start, "IWAD", 4) != 0) {
        fputs("DOOM V.Flash: chargez un ELF genere avec pack_wad.py.\n", stderr);
        _exit(1);
    }

    wad_data = (const unsigned char *)start;
    wad_size = length;
    wad_mode = (GameMode_t)mode;
    heap_limit = start;
    myargc = 1;
    myargv = argv;
    D_DoomMain();
    _exit(0);
}

void *_sbrk(ptrdiff_t delta)
{
    uintptr_t old = heap_break;
    if ((delta >= 0 && (uintptr_t)delta > heap_limit - old) ||
        (delta < 0 && (uintptr_t)0 - (uintptr_t)delta > old - (uintptr_t)__heap_start)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_break += delta;
    return (void *)old;
}

static struct session_file *session_file(const char *name)
{
    if (strcmp(name, "doom.cfg") == 0)
        return &config;

    if (strncmp(name, "doomsav", 7) == 0 && name[7] >= '0' &&
        name[7] < '0' + SAVE_SLOTS && strcmp(name + 8, ".dsg") == 0)
        return &saves[name[7] - '0'];

    return NULL;
}

int _open(const char *name, int flags, int mode)
{
    struct open_file file = {0};
    (void)mode;

    if (strcmp(name, "game.wad") == 0) {
        if (!wad_data || (flags & (O_ACCMODE | O_TRUNC)) != O_RDONLY) {
            errno = EACCES;
            return -1;
        }
        file.data = wad_data;
        file.size = wad_size;
    } else {
        file.save = session_file(name);
        if (!file.save || (!file.save->data && !(flags & O_CREAT))) {
            errno = ENOENT;
            return -1;
        }
    }

    for (unsigned index = 0; index < sizeof(files) / sizeof(files[0]); index++) {
        if (files[index].used)
            continue;

        if (file.save && (flags & O_APPEND) && !(flags & O_TRUNC))
            file.offset = file.save->size;
        file.flags = flags;
        file.used = 1;
        files[index] = file;
        return (int)index + 3;
    }

    errno = EMFILE;
    return -1;
}

static struct open_file *get_file(int fd)
{
    if (fd >= 3 && (unsigned)(fd - 3) < sizeof(files) / sizeof(files[0]) &&
        files[fd - 3].used)
        return &files[fd - 3];

    errno = EBADF;
    return NULL;
}

int _close(int fd)
{
    struct open_file *file = get_file(fd);
    if (!file)
        return -1;
    file->used = 0;
    return 0;
}

_ssize_t _read(int fd, void *data, size_t count)
{
    if (fd == STDIN_FILENO)
        return 0;
    struct open_file *file = get_file(fd);
    if (!file)
        return -1;
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        errno = EBADF;
        return -1;
    }

    size_t size = file->save ? file->save->size : file->size;
    const unsigned char *bytes = file->save ? file->save->data : file->data;
    if (file->offset >= size || !count)
        return 0;
    if (count > size - file->offset)
        count = size - file->offset;
    memcpy(data, bytes + file->offset, count);
    file->offset += count;
    return (_ssize_t)count;
}

_ssize_t _write(int fd, const void *data, size_t count)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        const unsigned char *bytes = data;
        for (size_t i = 0; i < count; i++) {
            while (!(UART1[5] & 0x20u))
                ;
            UART1[0] = bytes[i];
        }
        return (_ssize_t)count;
    }

    struct open_file *file = get_file(fd);
    if (!file)
        return -1;
    if (!file->save || (file->flags & O_ACCMODE) == O_RDONLY) {
        errno = EBADF;
        return -1;
    }
    if (!count)
        return 0;

    struct session_file *save = file->save;
    if ((file->flags & O_APPEND) && !(file->flags & O_TRUNC))
        file->offset = save->size;
    if (file->offset > SAVE_LIMIT || count > SAVE_LIMIT - file->offset) {
        errno = ENOSPC;
        return -1;
    }

    // Keep the prior save until there is enough memory for the replacement.
    size_t old_size = (file->flags & O_TRUNC) ? 0 : save->size;
    size_t end = file->offset + count;
    if (end > old_size) {
        unsigned char *bytes = realloc(save->data, end);
        if (!bytes) {
            errno = ENOMEM;
            return -1;
        }
        if (file->offset > old_size)
            memset(bytes + old_size, 0, file->offset - old_size);
        save->data = bytes;
        save->size = end;
    }
    file->flags &= ~O_TRUNC;
    memcpy(save->data + file->offset, data, count);
    file->offset = end;
    return (_ssize_t)count;
}

off_t _lseek(int fd, off_t offset, int whence)
{
    struct open_file *file = get_file(fd);
    if (!file)
        return -1;

    int64_t position = offset;
    switch (whence) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            position += file->offset;
            break;
        case SEEK_END:
            position += file->save ? file->save->size : file->size;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    if (position < 0 || position > INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    file->offset = position;
    return (off_t)position;
}

int _isatty(int fd)
{
    return fd >= 0 && fd < 3;
}

int _fstat(int fd, struct stat *info)
{
    memset(info, 0, sizeof(*info));
    if (_isatty(fd)) {
        info->st_mode = S_IFCHR;
        return 0;
    }

    struct open_file *file = get_file(fd);
    if (!file)
        return -1;
    info->st_mode = S_IFREG;
    info->st_size = file->save ? file->save->size : file->size;
    return 0;
}

int access(const char *name, int mode)
{
    const struct session_file *save = session_file(name);
    if ((save && save->data) || (wad_data && strcmp(name, "game.wad") == 0 && !(mode & W_OK)))
        return 0;

    errno = ENOENT;
    return -1;
}

int mkdir(const char *name, mode_t mode)
{
    (void)name;
    (void)mode;
    errno = ENOSYS;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int signal)
{
    (void)pid;
    (void)signal;
    errno = ENOSYS;
    return -1;
}
