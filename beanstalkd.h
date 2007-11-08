/* beanstalk.h - main header file */

#ifndef beanstalk_h
#define beanstalk_h

#define HOST INADDR_ANY
#define PORT 11300

#define CONSTSTRLEN(m) (sizeof(m) - 1)

#define MSG_INSERTED "INSERTED\r\n"
#define MSG_FOUND "FOUND"
#define MSG_NOTFOUND "NOT_FOUND\r\n"
#define MSG_DELETED "DELETED\r\n"
#define MSG_RELEASED "RELEASED\r\n"
#define MSG_BURIED "BURIED\r\n"

#define MSG_INSERTED_LEN CONSTSTRLEN(MSG_INSERTED)
#define MSG_NOTFOUND_LEN CONSTSTRLEN(MSG_NOTFOUND)
#define MSG_DELETED_LEN CONSTSTRLEN(MSG_DELETED)
#define MSG_RELEASED_LEN CONSTSTRLEN(MSG_RELEASED)
#define MSG_BURIED_LEN CONSTSTRLEN(MSG_BURIED)

#define CMD_PUT "put "
#define CMD_PEEK "peek"
#define CMD_PEEKJOB "peek "
#define CMD_RESERVE "reserve"
#define CMD_DELETE "delete "
#define CMD_RELEASE "release "
#define CMD_BURY "bury "
#define CMD_KICK "kick "
#define CMD_STATS "stats"
#define CMD_JOBSTATS "stats "

#define CMD_PEEK_LEN CONSTSTRLEN(CMD_PEEK)
#define CMD_PEEKJOB_LEN CONSTSTRLEN(CMD_PEEKJOB)
#define CMD_RESERVE_LEN CONSTSTRLEN(CMD_RESERVE)
#define CMD_DELETE_LEN CONSTSTRLEN(CMD_DELETE)
#define CMD_RELEASE_LEN CONSTSTRLEN(CMD_RELEASE)
#define CMD_BURY_LEN CONSTSTRLEN(CMD_BURY)
#define CMD_KICK_LEN CONSTSTRLEN(CMD_KICK)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)
#define CMD_JOBSTATS_LEN CONSTSTRLEN(CMD_JOBSTATS)

#define STATS_FMT "---\n" \
    "current-jobs-urgent: %u\n" \
    "current-jobs-ready: %u\n" \
    "current-jobs-reserved: %u\n" \
    "current-jobs-delayed: %u\n" \
    "current-jobs-buried: %u\n" \
    "cmd-put: %llu\n" \
    "cmd-peek: %llu\n" \
    "cmd-reserve: %llu\n" \
    "cmd-delete: %llu\n" \
    "cmd-release: %llu\n" \
    "cmd-bury: %llu\n" \
    "cmd-kick: %llu\n" \
    "cmd-stats: %llu\n" \
    "job-timeouts: %llu\n" \
    "current-connections: %u\n" \
    "current-producers: %u\n" \
    "current-workers: %u\n" \
    "current-waiting: %u\n" \
    "uptime: %u\n" \
    "\r\n"

#define JOB_STATS_FMT "---\n" \
    "id: %llu\n" \
    "state: %s\n" \
    "age: %u\n" \
    "delay: %u\n" \
    "time-left: %u\n" \
    "timeouts: %u\n" \
    "releases: %u\n" \
    "buries: %u\n" \
    "kicks: %u\n" \
    "\r\n"

#endif /*beanstalk_h*/
