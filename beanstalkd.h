/* beanstalk.h - main header file */

#ifndef beanstalk_h
#define beanstalk_h

#define HOST INADDR_ANY
#define PORT 3232

#define CSTRSZ(m) (sizeof(m) - 1)

#define MSG_INSERTED "INSERTED\r\n"
#define MSG_ERR "NOT_INSERTED\r\n"
#define MSG_NOTFOUND "NOT_FOUND\r\n"

#define CMD_PUT "put "
#define CMD_PEEK "peek "
#define CMD_RESERVE "reserve"
#define CMD_DELETE "delete "
#define CMD_STATS "stats "

#endif /*beanstalk_h*/
