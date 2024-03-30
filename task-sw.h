#ifndef TASK_SW_TASK_SW_H
#define TASK_SW_TASK_SW_H

#define MIN_DATA_SIZE           1
#define MAX_DATA_SIZE           128
#define MAGIC_SIZE              4
#define MAX_RETRIES             UINT_MAX
#define RETRY_INTERVAL_SEC      3

#define handle_error(msg)           do { perror(msg); log_close(); exit(EXIT_FAILURE); } while (0)
#define handle_error_en(en, msg)    do { errno = en; perror(msg); log_close(); exit(EXIT_FAILURE); } while (0)

//Function prototypes
void start_udp_server(void);
void start_tcp_connection();
void *connect_to_tcp_server(void *);
void *send_data_to_tcp_server(void *);
int parse_args(int, char*[]);
int is_valid_ipv4(const char *, const char*);
char *buf_to_hex_str(const char *, size_t);
void logger(const char *, ...);
void log_init(char*);
void log_close();
u_short str_to_u_short(const char *);

struct msg_to_tcp {
    ssize_t len;
    char* msg;
};

#endif //TASK_SW_TASK_SW_H
