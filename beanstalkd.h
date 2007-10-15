/* beanstalk.h - main header file */

#ifndef beanstalk_h
#define beanstalk_h

#define HOST INADDR_ANY
#define PORT 11300

#define CONSTSTRLEN(m) (sizeof(m) - 1)

#define MSG_INSERTED "INSERTED\r\n"
#define MSG_NOT_INSERTED "NOT_INSERTED\r\n"
#define MSG_FOUND "FOUND"
#define MSG_NOTFOUND "NOT_FOUND\r\n"
#define MSG_DELETED "DELETED\r\n"
#define MSG_RELEASED "RELEASED\r\n"
#define MSG_BURIED "BURIED\r\n"
#define MSG_NOT_RELEASED "NOT_RELEASED\r\n"

#define MSG_INSERTED_LEN CONSTSTRLEN(MSG_INSERTED)
#define MSG_NOT_INSERTED_LEN CONSTSTRLEN(MSG_NOT_INSERTED)
#define MSG_NOTFOUND_LEN CONSTSTRLEN(MSG_NOTFOUND)
#define MSG_DELETED_LEN CONSTSTRLEN(MSG_DELETED)
#define MSG_RELEASED_LEN CONSTSTRLEN(MSG_RELEASED)
#define MSG_BURIED_LEN CONSTSTRLEN(MSG_BURIED)
#define MSG_NOT_RELEASED_LEN CONSTSTRLEN(MSG_NOT_RELEASED)

#define CMD_PUT "put "
#define CMD_PEEK "peek "
#define CMD_RESERVE "reserve"
#define CMD_DELETE "delete "
#define CMD_RELEASE "release "
#define CMD_BURY "bury "
#define CMD_KICK "kick "
#define CMD_STATS "stats"
#define CMD_JOBSTATS "stats "

#define CMD_PEEK_LEN CONSTSTRLEN(CMD_PEEK)
#define CMD_RESERVE_LEN CONSTSTRLEN(CMD_RESERVE)
#define CMD_DELETE_LEN CONSTSTRLEN(CMD_DELETE)
#define CMD_RELEASE_LEN CONSTSTRLEN(CMD_RELEASE)
#define CMD_BURY_LEN CONSTSTRLEN(CMD_BURY)
#define CMD_KICK_LEN CONSTSTRLEN(CMD_KICK)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)
#define CMD_JOBSTATS_LEN CONSTSTRLEN(CMD_JOBSTATS)

#define STATS_FMT "---\n" \
    "current-jobs-urgent: %d\n" \
    "current-jobs-ready: %d\n" \
    "current-jobs-reserved: %d\n" \
    "current-jobs-buried: %d\n" \
    "cmd-put: %lld\n" \
    "cmd-peek: %lld\n" \
    "cmd-reserve: %lld\n" \
    "cmd-delete: %lld\n" \
    "cmd-release: %lld\n" \
    "cmd-bury: %lld\n" \
    "cmd-kick: %lld\n" \
    "cmd-stats: %lld\n" \
    "job-timeouts: %lld\n" \
    "current-connections: %d\n" \
    "current-producers: %d\n" \
    "current-workers: %d\n" \
    "\r\n"

#define JOB_STATS_FMT "---\n" \
    "id: %lld\n" \
    "state: %s\n" \
    "age: %d\n" \
    "time-left: %d\n" \
    "timeouts: %d\n" \
    "\r\n"

#endif /*beanstalk_h*/
