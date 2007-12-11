/* reserve.h - job reservations header */

/* Copyright (C) 2007 Keith Rarick and Philotic Inc.

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

#ifndef reserve_h
#define reserve_H

#include "job.h"
#include "conn.h"

void reserve_job(conn c, job j);
int has_reserved_job(conn c);
job soonest_job(conn c);
void enqueue_reserved_jobs(conn c);
int has_reserved_this_job(conn c, job j);
job remove_reserved_job(conn c, unsigned long long int id);
job remove_this_reserved_job(conn c, job j);
job find_reserved_job(unsigned long long int id);
job find_reserved_job_in_list(conn list, unsigned long long int id);

unsigned int get_reserved_job_ct();

#endif /*reserve_h*/
