/* binlog.h - binary log implementation */

/* Copyright (C) 2008 Graham Barr

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef binlog_h
#define binlog_h

#include "job.h"

extern char *binlog_dir;
extern size_t binlog_size_limit;
#define BINLOG_SIZE_LIMIT_DEFAULT (10 << 20)

extern int enable_fsync;
extern size_t fsync_throttle_ms;

void binlog_init(job binlog_jobs);

/* Return the number of locks acquired: either 0 or 1. */
int binlog_lock();

/* Returns the number of jobs successfully written (either 0 or 1). */
int binlog_write_job(job j);
size_t binlog_reserve_space_put(job j);
size_t binlog_reserve_space_update(job j);

void binlog_shutdown();
const char *binlog_oldest_index();
const char *binlog_current_index();

#endif

