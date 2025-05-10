#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

// Function to parse the FTP URL
int parse_ftp_url(const char *url, char *user, char *password, char *host, char *path) {
    char temp_url[BUFFER_SIZE];
    strncpy(temp_url, url, BUFFER_SIZE - 1);
    temp_url[BUFFER_SIZE - 1] = '\0';

    if (strncmp(temp_url, "ftp://", 6) != 0) {
        fprintf(stderr, "Invalid URL: must start with ftp://\n");
        return -1;
    }

    char *auth_host_path = temp_url + 6; // Skip "ftp://"
    char *at_sign = strchr(auth_host_path, '@');

    if (at_sign) {
        *at_sign = '\0';
        char *auth = auth_host_path;
        auth_host_path = at_sign + 1;

        char *colon = strchr(auth, ':');
        if (colon) {
            *colon = '\0';
            strncpy(user, auth, BUFFER_SIZE - 1);
            user[BUFFER_SIZE - 1] = '\0';
            strncpy(password, colon + 1, BUFFER_SIZE - 1);
            password[BUFFER_SIZE - 1] = '\0';
        } else {
            strncpy(user, auth, BUFFER_SIZE - 1);
            user[BUFFER_SIZE - 1] = '\0';
            strncpy(password, "", BUFFER_SIZE - 1);
            password[BUFFER_SIZE - 1] = '\0';
        }
    } else {
        strncpy(user, "anonymous", BUFFER_SIZE - 1);
        user[BUFFER_SIZE - 1] = '\0';
        strncpy(password, "anonymous@", BUFFER_SIZE - 1);
        password[BUFFER_SIZE - 1] = '\0';
    }

    char *slash = strchr(auth_host_path, '/');
    if (!slash) {
        fprintf(stderr, "Invalid URL: must contain a path\n");
        return -1;
    }

    *slash = '\0';
    strncpy(host, auth_host_path, BUFFER_SIZE - 1);
    host[BUFFER_SIZE - 1] = '\0';
    strncpy(path, slash + 1, BUFFER_SIZE - 1);
    path[BUFFER_SIZE - 1] = '\0';

    return 0;
}

// Function to extract the file name from the path
const char* extract_file_name(const char *path) {
    const char *file_name = strrchr(path, '/');
    if (file_name) {
        return file_name + 1;
    }
    return path;
}

// Function to establish a connection to the server
int connect_to_server(const char *host, int port) {
    struct hostent *server;
    struct sockaddr_in server_addr;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        return -1;
    }

    server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "Error: no such host\n");
        close(sockfd);
        return -1;
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Function to read a complete FTP server response
int read_response(int sockfd, char *response) {
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    bzero(response, BUFFER_SIZE);

    while (1) {
        if (read(sockfd, buffer, BUFFER_SIZE - 1) < 0) {
            perror("Error reading from socket");
            return -1;
        }

        strcat(response, buffer);

        // Check if the response is complete (ends with the same code followed by a space)
        char *line = strtok(response, "\n");
        while (line != NULL) {
            if (strlen(line) >= 4 && line[3] == ' ') {
                return 0;
            }
            line = strtok(NULL, "\n");
        }
    }
}

// Function to send a command and receive a response
int send_command(int sockfd, const char *command, char *response) {
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    snprintf(buffer, BUFFER_SIZE - 1, "%s\r\n", command); // Ensure \r\n termination
    buffer[BUFFER_SIZE - 1] = '\0';
    if (write(sockfd, buffer, strlen(buffer)) < 0) {
        perror("Error writing to socket");
        return -1;
    }

    return read_response(sockfd, response);
}

// Function to parse the PASV response
int parse_pasv_response(const char *response, char *data_host, int *data_port) {
    int pasv_ip[4], pasv_port[2];
    if (strstr(response, "227") == NULL) {
        fprintf(stderr, "Error: PASV command not acknowledged: %s\n", response);
        return -1;
    }

    if (sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &pasv_ip[0], &pasv_ip[1], &pasv_ip[2], &pasv_ip[3], &pasv_port[0], &pasv_port[1]) != 6) {
        fprintf(stderr, "Error parsing PASV response: %s\n", response);
        return -1;
    }

    snprintf(data_host, BUFFER_SIZE - 1, "%d.%d.%d.%d", pasv_ip[0], pasv_ip[1], pasv_ip[2], pasv_ip[3]);
    data_host[BUFFER_SIZE - 1] = '\0';
    *data_port = pasv_port[0] * 256 + pasv_port[1];

    return 0;
}

// Main function implementing the FTP client
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char user[BUFFER_SIZE], password[BUFFER_SIZE], host[BUFFER_SIZE], path[BUFFER_SIZE];
    if (parse_ftp_url(argv[1], user, password, host, path) < 0) {
        exit(EXIT_FAILURE);
    }

    printf("Connecting to host: %s\n", host);
    int control_sock = connect_to_server(host, 21);
    if (control_sock < 0) {
        exit(EXIT_FAILURE);
    }

    char response[BUFFER_SIZE];
    if (read_response(control_sock, response) < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server response: %s\n", response);

    char command[BUFFER_SIZE];

    snprintf(command, BUFFER_SIZE - 1, "USER %s", user);
    command[BUFFER_SIZE - 1] = '\0';
    if (send_command(control_sock, command, response) < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }
    printf("%s\n", response);

    snprintf(command, BUFFER_SIZE - 1, "PASS %s", password);
    command[BUFFER_SIZE - 1] = '\0';
    if (send_command(control_sock, command, response) < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }
    printf("%s\n", response);

    if (send_command(control_sock, "PASV", response) < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }
    printf("%s\n", response);

    char data_host[BUFFER_SIZE];
    int data_port;
    if (parse_pasv_response(response, data_host, &data_port) < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }

    printf("Connecting to data host: %s:%d\n", data_host, data_port);
    int data_sock = connect_to_server(data_host, data_port);
    if (data_sock < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }

    snprintf(command, BUFFER_SIZE - 1, "RETR %s", path);
    command[BUFFER_SIZE - 1] = '\0';
    if (send_command(control_sock, command, response) < 0) {
        close(control_sock);
        close(data_sock);
        exit(EXIT_FAILURE);
    }
    printf("%s\n", response);

    const char *file_name = extract_file_name(path);
    FILE *file = fopen(file_name, "wb");
    if (!file) {
        perror("Error opening file");
        close(control_sock);
        close(data_sock);
        exit(EXIT_FAILURE);
    }

    char data_buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = read(data_sock, data_buffer, BUFFER_SIZE)) > 0) {
        fwrite(data_buffer, 1, bytes_read, file);
    }

    fclose(file);
    close(data_sock);

    if (read_response(control_sock, response) < 0) {
        close(control_sock);
        exit(EXIT_FAILURE);
    }
    printf("%s\n", response);

    send_command(control_sock, "QUIT", response);
    printf("%s\n", response);

    close(control_sock);
    return 0;
}