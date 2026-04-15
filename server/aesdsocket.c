/* File        : aesdsocket.c
 * Author      : Wei Jiun Lim
 * Descriptiom : For AELD Assignment 5 Part 1 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Debug prints
 * Note: do not enable if running in Daemon mode */
#define DEBUG_LOG(msg,...)
// #define DEBUG_LOG(msg,...) printf("aesdsocket: " msg "\n" , ##__VA_ARGS__)

#define RECV_BUF_SIZE (1024)
#define SEND_BUF_SIZE (1024)

static void signal_handler(int signal_number);

volatile sig_atomic_t signal_received = 0;

static void signal_handler(int signal_number)
{
    if ((signal_number == SIGINT) || (signal_number == SIGTERM)) {
        signal_received = 1;
    }
}

int main(int argc, char *argv[]) {

    bool daemon_mode = false;
    int result = 0;
    int status;
    FILE *fp;
    struct sigaction new_action;
    char aesdsocketdata_file[] = "/var/tmp/aesdsocketdata";
    char recv_buf[RECV_BUF_SIZE];
    char *accum_buf = NULL;
    size_t accum_len = 0;
    int sockfd = -1;
    int acceptedfd = -1;

    /* Open syslog */
    openlog(argv[0], LOG_CONS | LOG_PID, LOG_USER);

    /* Check for daemon mode */
    if (argc == 2) {
        if (strcmp(argv[1], "-d")  == 0) {
            daemon_mode = true;
        }
    }

    /* Register SIGTERM and SIGINT */
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &new_action, NULL) != 0) {
        DEBUG_LOG("Error registering SIGTERM, %d (%s)!", errno, strerror(errno));
        return (1);        
    }

    if (sigaction(SIGINT, &new_action, NULL) != 0) {
        DEBUG_LOG("Error registering SIGINT, %d (%s)!", errno, strerror(errno));
        return (1);        
    }

    /* Get File Descriptor for new socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        DEBUG_LOG("Socket error, sockfd = %d!", sockfd);
        syslog (LOG_ERR, "Socket error, sockfd = %d!", sockfd);
        result = 1;
        goto end;
    }

    /* Get addrinfo */
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo *addrinfo_res_ptr;
    status = getaddrinfo(NULL, "9000", &hints, &addrinfo_res_ptr);

    if (status != 0) {
        DEBUG_LOG("getaddrinfo() error, %s!", gai_strerror(status));
        syslog (LOG_ERR, "getaddrinfo() error, %s!", gai_strerror(status));
        result = 1;
        goto end_after_sockfd;
    }

    /* Enable faster reuse of address and port after a restart */
    int opt = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (status < 0) {
        DEBUG_LOG("setsockopt() error, %s!", strerror(errno));
        syslog (LOG_ERR, "setsockopt() error, %s!", strerror(errno));
        result = 1;
        goto end_after_sockfd;
    }

    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    if (status < 0) {
        DEBUG_LOG("setsockopt() error, %s!", strerror(errno));
        syslog (LOG_ERR, "setsockopt() error, %s!", strerror(errno));
        result = 1;
        goto end_after_sockfd;
    }

    /* Bind socket to address */
    status = bind(sockfd, addrinfo_res_ptr->ai_addr, addrinfo_res_ptr->ai_addrlen);

    /* Free addrinfo struct */
    freeaddrinfo(addrinfo_res_ptr);

    if (status < 0) {
        DEBUG_LOG("bind() error, %s!", strerror(errno));
        syslog (LOG_ERR, "bind() error, %s!", strerror(errno));
        result = 1;
        goto end_after_sockfd;
    }

    /* Start daemon mode if required */
    if (daemon_mode) {
        
        /* Create a child process */
        pid_t pid = fork();

        if (pid < 0) {

            syslog (LOG_ERR, "fork() error, %s!", strerror(errno));
            result = 1;
            goto end_after_sockfd;            
        }
        else if (pid > 0) {

            /* Fork succeeded, this is the parent */
            result = 0;
            goto end_after_sockfd; 
        }

        /* If we are here, pid == 0, fork succeeded this is the child process */

        /* Set session ID to detach child process from terminal, makes process session leader */
        if (setsid() <0) {
            syslog (LOG_ERR, "setsid() error, %s!", strerror(errno));
            result = 1;
            goto end_after_sockfd;             
        }

        /* Fork a second time to ensure daemon doesn't reacquire terminal */
        pid = fork();

        if (pid < 0) {

            syslog (LOG_ERR, "fork() error, %s!", strerror(errno));
            result = 1;
            goto end_after_sockfd;            
        }
        else if (pid > 0) {

            /* Fork succeeded, this is the parent */
            result = 0;
            goto end_after_sockfd; 
        }

        /* If we are here, pid == 0, second fork succeeded this is the child process */

        /* Change working directory */
        chdir("/");

        /* Reset file permission mask */
        umask(0);

        /* Close terminal IOs */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    /* Listen to socket */
    status = listen(sockfd, 1);  

    if (status < 0) {
        DEBUG_LOG("listen() error, %s!", strerror(errno));
        syslog (LOG_ERR, "listen() error, %s!", strerror(errno));
        result = 1;
        goto end_after_sockfd;
    }  

    while (1) {

        /* Accept connection */
        DEBUG_LOG("Accepting ...");
        struct sockaddr_in sockaddr_conn;
        socklen_t addrlen = sizeof(sockaddr_conn);
        acceptedfd = accept(sockfd, (struct sockaddr *) &sockaddr_conn, &addrlen);

        if (acceptedfd < 0) {

            if (errno == EINTR) {
                /* Interrupted by Signal */
                DEBUG_LOG("accept() interrupted by Signal, EINTR");
                goto check_signal;
            }

            DEBUG_LOG("accept() error, %s!", strerror(errno));
            syslog (LOG_ERR, "accept() error, %s!", strerror(errno));
            result = 1;
            goto end_after_sockfd;
        }    

        /* Convert IP address to string for logging */
        char ipstr[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &sockaddr_conn.sin_addr, ipstr, sizeof(ipstr))) {
            strcpy(ipstr, "unknown");
        }
        DEBUG_LOG("Accepted connection from %s", ipstr);
        syslog(LOG_DEBUG, "Accepted connection from %s", ipstr);

        
        /* Receive */
        accum_len = 0;
        while (1) {

            ssize_t recv_bytes = recv(acceptedfd, recv_buf, RECV_BUF_SIZE, 0);

            if (recv_bytes == 0) {
                /* Connection closed */
                break;
            }

            if (recv_bytes < 0) {

                if (errno == EINTR) {
                    /* Interrupted by Signal, restart loop */
                    DEBUG_LOG("recv() interrupted by Signal, EINTR");
                    continue;
                }

                /* Error occured */
                DEBUG_LOG("recv() error, recv_bytes = %zd!", recv_bytes);
                syslog(LOG_ERR, "recv() error, recv_bytes = %zd!", recv_bytes);
                result = 1;
                goto end_after_acceptedfd;     
            }

            /* Allocate space for accum buffer */
            char *new_buf = realloc(accum_buf, accum_len + recv_bytes);
            if (!new_buf) {
                /* malloc failure */
                free(accum_buf);
                accum_buf = NULL;
                accum_len = 0;
                DEBUG_LOG("accum_buf malloc error");
                syslog(LOG_ERR, "accum_buf malloc error");
                continue;
            }
            accum_buf = new_buf;

            /* Append to accumulation buffer */
            memcpy(accum_buf + accum_len, recv_buf, recv_bytes);
            accum_len += recv_bytes;

            /* Process completed lines */
            size_t start = 0;
            for (size_t idx = 0; idx < accum_len; idx++) {

                if (accum_buf[idx] == '\n') {

                    size_t line_len = idx - start + 1;
                    
                    /* Open file, or create new file if it does not exist */
                    fp = fopen(aesdsocketdata_file, "a");
                    if (fp == NULL) {
                        DEBUG_LOG("Error opening a file %s!", aesdsocketdata_file);
                        syslog (LOG_ERR, "Error opening a file %s!", aesdsocketdata_file);
                        result = 1; 
                        goto end_after_acceptedfd;
                    }

                    /* Append completed line to file */
                    if (fprintf(fp, "%.*s", (int)line_len, accum_buf + start) < 0) {
                        fclose(fp);
                        DEBUG_LOG("Error writing file %s!", aesdsocketdata_file);
                        syslog(LOG_ERR, "Error writing file %s!", aesdsocketdata_file);
                        result = 1;
                        goto end_after_acceptedfd;     
                    }
                    fclose(fp);

                    DEBUG_LOG("Writing %.*s to %s", (int)line_len, accum_buf + start, aesdsocketdata_file);
                    syslog(LOG_DEBUG, "Writing %.*s to %s", (int)line_len, accum_buf + start, aesdsocketdata_file);

                    start = idx + 1;

                    /* Send data file back */
                    {
                        char send_buf[SEND_BUF_SIZE];
                        size_t bytes_read;

                        fp = fopen(aesdsocketdata_file, "r");
                        if (fp == NULL) {
                            DEBUG_LOG("Error opening rb file %s!", aesdsocketdata_file);
                            syslog (LOG_ERR, "Error opening rb file %s!", aesdsocketdata_file);
                            result = 1; 
                            goto end_after_acceptedfd;
                        }

                        while ((bytes_read = fread(send_buf, 1, SEND_BUF_SIZE, fp)) > 0) {

                            size_t total_sent = 0;

                            DEBUG_LOG("Read %.*s from %s", (int)bytes_read, send_buf, aesdsocketdata_file);

                            while (total_sent < bytes_read) {

                                ssize_t sent = send(acceptedfd,
                                                    send_buf + total_sent,
                                                    bytes_read - total_sent,
                                                    0);

                                if (sent <= 0) {

                                    if (errno == EINTR) {
                                        /* Interrupted by Signal, restart loop */
                                        DEBUG_LOG("send() interrupted by Signal, EINTR");
                                        continue;
                                    }

                                    fclose(fp);
                                    DEBUG_LOG("Error sending, sent = %zd", sent);
                                    syslog (LOG_ERR, "Error sending, sent = %zd", sent);
                                    result = 1; 
                                    goto end_after_acceptedfd;
                                }

                                DEBUG_LOG("Sent %.*s", (int)sent, send_buf + total_sent);

                                total_sent += sent;
                            }
                        }
                        fclose(fp);
                    }
                }
            }

            /* Move leftover (incomplete data) to front */
            if (start > 0) {
                memmove(accum_buf, accum_buf + start, accum_len - start);
                accum_len -= start;
            }
        }
        free(accum_buf);
        accum_buf = NULL;
        accum_len = 0;

        /* Close connection */
        if (acceptedfd != -1) {
            close(acceptedfd);
            acceptedfd = -1;
            DEBUG_LOG("Closed connection from %s", ipstr);
            syslog(LOG_DEBUG, "Closed connection from %s", ipstr);
        }

check_signal:
        /* Exit if SIGTERM or SIGINT received */
        if (signal_received) {

            DEBUG_LOG("Caught signal, exiting");
            syslog(LOG_DEBUG, "Caught signal, exiting");

            goto end_after_sockfd;
        }
    }

end_after_acceptedfd:
    if (acceptedfd != -1) {
        close(acceptedfd);
        acceptedfd = -1;
    }

end_after_sockfd:
    close(sockfd);

end:
    /* Delete the data file if exists */
    fp = fopen(aesdsocketdata_file, "r");
    if (fp) {
        fclose(fp);
        if (remove(aesdsocketdata_file) < 0) {
            DEBUG_LOG("Error deleting file %s!", aesdsocketdata_file);
            syslog(LOG_ERR, "Error deleting file %s!", aesdsocketdata_file);
            return (1);        
        }
    }

    if (accum_buf) {
        free(accum_buf);
        accum_buf = NULL;
        accum_len = 0;
    }

    closelog();

    return (result); 
}
