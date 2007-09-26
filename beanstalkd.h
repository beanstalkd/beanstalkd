/* beanstalk.h - main header file */

#ifndef beanstalk_h
#define beanstalk_h

#define HOST INADDR_ANY
#define PORT 3232

#define CONSTSTRLEN(m) (sizeof(m) - 1)

#define MSG_INSERTED "INSERTED\r\n"
#define MSG_NOT_INSERTED "NOT_INSERTED\r\n"
#define MSG_FOUND "FOUND"
#define MSG_NOTFOUND "NOT_FOUND\r\n"
#define MSG_DELETED "DELETED\r\n"

#define MSG_INSERTED_LEN CONSTSTRLEN(MSG_INSERTED)
#define MSG_NOT_INSERTED_LEN CONSTSTRLEN(MSG_NOT_INSERTED)
#define MSG_NOTFOUND_LEN CONSTSTRLEN(MSG_NOTFOUND)
#define MSG_DELETED_LEN CONSTSTRLEN(MSG_DELETED)

#define CMD_PUT "put "
#define CMD_PEEK "peek "
#define CMD_RESERVE "reserve"
#define CMD_DELETE "delete "
#define CMD_STATS "stats"
#define CMD_JOBSTATS "stats "

#define CMD_PEEK_LEN CONSTSTRLEN(CMD_PEEK)
#define CMD_RESERVE_LEN CONSTSTRLEN(CMD_RESERVE)
#define CMD_DELETE_LEN CONSTSTRLEN(CMD_DELETE)
#define CMD_STATS_LEN CONSTSTRLEN(CMD_STATS)

/* plenty big enough to hold the stats output */
#define STATS_BUF_SIZE 400
#define STATS_FMT "OK xxx\r\n" \
    "---\n" \
    "current-jobs-ready: %d\n" \
    "current-jobs-reserved: %d\n" \
    "cmd-put: %lld\n" \
    "cmd-peek: %lld\n" \
    "cmd-reserve: %lld\n" \
    "cmd-delete: %lld\n" \
    "cmd-stats: %lld\n" \
    "job-timeouts: %lld\n" \
    "current-connections: %d\n" \
    "current-producers: %d\n" \
    "current-workers: %d\n" \
    ""

#endif /*beanstalk_h*/
