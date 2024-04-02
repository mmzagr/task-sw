#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include "task-sw.h"

//Global variables
int connected = 0;
int tcp_socket;
char udp_ip[INET_ADDRSTRLEN], tcp_ip[INET_ADDRSTRLEN], log_file_name[PATH_MAX], magic_bytes[MAGIC_SIZE];
unsigned short udp_port, tcp_port;
pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char* argv[]) {
    if (!parse_args(argc, argv))
        exit(EXIT_FAILURE);

    log_init(log_file_name);
    printf("%s version %s started ok, pid %d, all further output will be redirected to the log file: %s\n", argv[0], VERSION, getpid(), log_file_name);
    logger("started successfully, pid %d", getpid());

    start_tcp_connection();
    start_udp_server();
    log_close();
    return 0;
}

void start_udp_server(void) {
    int serv_socket, on = 1;
    struct sockaddr_in serv_addr, clnt_addr;
    char rcvd_buf[MAX_DATA_SIZE + MAGIC_SIZE];
    pthread_t tid;
    socklen_t clnt_addr_len = sizeof(clnt_addr);

    memset(&serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, udp_ip, &serv_addr.sin_addr.s_addr);
    serv_addr.sin_port = htons(udp_port);

    logger("starting UDP server at at %s:%d", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
    if ((serv_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        handle_error("socket");
    if (setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        handle_error("setsockopt");
    if (bind(serv_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        handle_error("bind");

    char *hex_buf = buf_to_hex_str(magic_bytes, sizeof(magic_bytes));
    logger("UDP server has started ok, using '%s' %s as magic bytes for the injection", magic_bytes, hex_buf);
    free(hex_buf);

    memcpy(rcvd_buf, magic_bytes, MAGIC_SIZE);                                               //add magic bytes at the beginning of the buffer
    while (1) {
        memset(rcvd_buf + MAGIC_SIZE, '\0', MAX_DATA_SIZE);                                  //clear the data-part of the buffer
        memset(&clnt_addr, '\0', clnt_addr_len);
        ssize_t n = recvfrom(serv_socket, rcvd_buf + MAGIC_SIZE, MAX_DATA_SIZE, 0, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
        if (n == -1) {
            logger("recvfrom: %s", strerror(errno));
            continue;
        }
        if (n < MIN_DATA_SIZE) {
            logger("received UDP data is too small (%zd < %d) bytes and won't be processed", n, MIN_DATA_SIZE);
            continue;
        }
        hex_buf = buf_to_hex_str(rcvd_buf + MAGIC_SIZE, n);
        logger("received %zd bytes by UDP from: %s:%d: %s", n, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port), hex_buf);
        free(hex_buf);

        if (!connected) {
            logger("tcp socket is not connected, the data won't be sent to the TCP server");
            continue;
        }

        char *rcvd_buf_cpy = (char*)calloc(n + MAGIC_SIZE, sizeof(char));                    //creating a copy of the message for the sending thread
        memcpy(rcvd_buf_cpy, rcvd_buf, n + MAGIC_SIZE);

        struct msg_to_tcp thread_args = {n + MAGIC_SIZE, rcvd_buf_cpy};
        int result = pthread_create(&tid, NULL, send_data_to_tcp_server, (void *)&thread_args);
        if (result != 0)
            handle_error_en(result, "pthread_create");
    }
}

void *send_data_to_tcp_server(void *arg) {                                                    //run as a separate thread
    struct msg_to_tcp* msgToTcp = (struct msg_to_tcp*)arg;
    char *msg = msgToTcp -> msg;
    ssize_t len = msgToTcp -> len;

    pthread_mutex_lock(&socket_mutex);

    ssize_t bytes_send = send(tcp_socket, msg, len, 0);
    if (bytes_send == -1)
        logger("could not send data to the TCP server: %s", strerror(errno));
    else {
        char *hex_bytes = buf_to_hex_str(msg, len);
        logger("data of %zd bytes was sent to the TCP server: %s", len, hex_bytes);
        free(hex_bytes);
    }
    pthread_mutex_unlock(&socket_mutex);
    free(msg);
    return NULL;
}

void start_tcp_connection() {
    pthread_t tid;
    int result = pthread_create(&tid, NULL, connect_to_tcp_server, NULL);
    if (result != 0)
        handle_error_en(result, "pthread_create");
}

void *connect_to_tcp_server(void *arg) {
    int retries = 0;
    struct sockaddr_in serv_addr;
    char buf[MAX_DATA_SIZE];
    fd_set read_sd;

    // Configure server address
    memset(&serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(tcp_port);
    inet_pton(AF_INET, tcp_ip, &serv_addr.sin_addr.s_addr);

    while (retries < MAX_RETRIES) {
        if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            handle_error("socket");

        logger("connecting to a TCP server %s:%d, %u attempts left", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port), MAX_RETRIES - retries);
        retries++;
        if (connect(tcp_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
            logger("connect: %s", strerror(errno));
            if (retries < MAX_RETRIES) {
                logger("sleeping %d seconds", RETRY_INTERVAL_SEC);
                sleep(RETRY_INTERVAL_SEC);
            }
            continue;
        }
        connected = 1;
        logger("TCP server connection ok");

        while (1) {
            FD_ZERO(&read_sd);
            FD_SET(tcp_socket, &read_sd);
            int sel = select(tcp_socket + 1, &read_sd, NULL, NULL, NULL);
            if (sel == -1) {                                                                     //select error
                connected = 0;
                close(tcp_socket);
                logger("select: %s", strerror(errno));
                break;
            }

            ssize_t bytes = recv(tcp_socket, &buf, sizeof(buf), 0);
            if (bytes == -1) {                                                                    //recv error, network error, interrupted, etc...
                connected = 0;
                close(tcp_socket);
                logger("recv:", strerror(errno));
                break;
            }

            if (bytes == 0) {                                                                    //TCP connection was closed gracefully
                connected = 0;
                close(tcp_socket);
                logger("TCP server has closed connection");
                break;
            }

            logger("data received from a TCP served (%zd bytes) was silently ignored", bytes);                   //bytes > 0
        }
    }
    logger("maximum retry attempts (%d) has been reached, no more reconnections will be made", MAX_RETRIES);
    close(tcp_socket);
    return NULL;
}

int parse_args(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s <UDP IP> <UDP port> <TCP IP> <TCP port> <log file name> <magic bytes>\n", argv[0]);
        return 0;
    }

    if (!is_valid_ipv4(argv[1], "UDP_IP")) {
        fprintf(stderr, "UDP IP %s is not a valid IPV4 address, exiting\n", argv[1]);
        return 0;
    }
    strncpy(udp_ip, argv[1], INET_ADDRSTRLEN);

    if ((udp_port = str_to_u_short(argv[2])) == 0) {
        fprintf(stderr, "invalid UDP port: %s, should be (1-65535) exiting...\n", argv[2]);
        return 0;
    }

    if (!is_valid_ipv4(argv[3], "TCP_IP")) {
        fprintf(stderr, "TCP IP %s is not a valid IPV4 address, exiting\n", argv[3]);
        return 0;
    }
    strncpy(tcp_ip, argv[3], INET_ADDRSTRLEN);

    if ((tcp_port = str_to_u_short(argv[4])) == 0) {
        fprintf(stderr, "invalid TCP port: %s, should be (1-65535), exiting\n", argv[4]);
        return 0;
    }

    if (sizeof(log_file_name) < strlen(argv[5]) + 1) {
        fprintf(stderr, "log file name provided is too long, exiting");
        return 0;
    }
    strcpy(log_file_name, argv[5]);

    if (strlen(argv[6]) != MAGIC_SIZE) {
        fprintf(stderr, "magic size (%zu bytes) is not required %d bytes long, exiting", strlen(argv[6]), MAGIC_SIZE);
        return 0;
    }
    memcpy(magic_bytes, argv[6], MAGIC_SIZE);
    return 1;
}
