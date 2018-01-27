/**
 * kalu - Copyright (C) 2012-2016 Olivier Brunel
 *
 * serialize.c
 * Copyright (C) 2018 Olivier Brunel <jjk@jjacky.com>
 *
 * This file is part of kalu.
 *
 * kalu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * kalu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * kalu. If not, see http://www.gnu.org/licenses/
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define PROG    "serialize"

static void usage (int rc)
{
    puts ("Usage: " PROG " NAME INPUT\n");
    puts ("Will read INPUT and print on stdout a corresponding C file serialization\n"
          "with symbol NAME as const char[] and NAME_size as size_t");
    _exit (rc);
}

static void fatal (int rc, const char *msg)
{
    printf (PROG ": fatal error: %s: %s\n", msg, strerror (errno));
    _exit (rc);
}

int main (int argc, const char *argv[])
{
    int fd;
    off_t len;

    if (argc != 3)
        usage (1);

    fd = open (argv[2], O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        fatal (2, "failed to open input");

    len = lseek (fd, 0, SEEK_END);
    if (len < 0)
        fatal (2, "failed to seek to end of input");
    if (lseek (fd, 0, SEEK_SET) < 0)
        fatal (2, "failed to seek to beginning of input");

    printf ("#include <sys/types.h>\nconst char %s[] = { ", argv[1]);

    for (;;)
    {
        char buf[len], *b = buf;
        ssize_t r;

        r = read (fd, b, len);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            fatal (3, "failed to read input");
        }
        if (r == 0)
            break;

        for ( ; r > 0; --r, ++b)
            printf ("%s0x%.2hhx", (b == buf) ? "" : ", ", (unsigned int) *b);

        len -= r;
    }

    printf (" };\nsize_t %s_size = sizeof (%s) / sizeof (*%s);", argv[1], argv[1], argv[1]);

    close (fd);
    return 0;
}
