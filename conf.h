/*
 * conf.h
 * Copyright (C) 2020 Adrian Perez de Castro <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef CONF_H
#define CONF_H

#include <stdbool.h>
#include <stdio.h>

struct cflag;
struct dbuf;

extern bool conf_parse(FILE               *input,
                       const struct cflag *specs,
                       struct dbuf        *err);

#endif /* !CONF_H */
