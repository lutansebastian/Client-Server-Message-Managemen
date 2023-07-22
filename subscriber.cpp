#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <netinet/tcp.h>
#include <poll.h>
#include <fcntl.h>

using namespace std;

int main(int argc, char *argv[]) {

    // Verific numarul argumentelor
    if (argc != 4) {
        throw runtime_error("[client] Eroare la argumente");
    }

    char server_message[1501], client_message[1501];

    memset(server_message,'\0',sizeof(server_message));
    memset(client_message,'\0',sizeof(client_message));
  
    // Crearea socketului de TCP
    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc < 0){
        throw runtime_error("[client] Eroare la crearea socketului TCP");
    }
    
    // Dezactivarea algoritmului Nagle
    int nagle_off = 1;
    if (setsockopt(socket_desc, IPPROTO_TCP, TCP_NODELAY, (char *)&nagle_off, sizeof(nagle_off)) < 0) {
        throw runtime_error("[client] Eroare la Nagle");
    }

    // Salvez portul serverului
    struct sockaddr_in server_addr;
    int server_port = atoi(argv[3]);
    memset(&server_addr, 0, sizeof(server_addr));

    // Setez portul si IP-ul la fel ca la server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    if (inet_aton(argv[2], &server_addr.sin_addr) == 0) {
        throw runtime_error("[client] Eroare la adresa IP");
    }

    // Conectarea la server
    if(connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        throw runtime_error("[client] Eroare la conectare");
    }

    if (send(socket_desc, argv[1], strlen(argv[1]), 0) < 0) {
        throw runtime_error("[client] Eroare la trimiterea ID-ului catre client");
    }
    
    // Setez file descriptorii ca non-blocanti
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    fcntl(socket_desc, F_SETFL, O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = socket_desc;
    fds[1].events = POLLIN;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    while (1) {
        // Daca serverul nu mai trimite niciun mesaj, inchid clientii.
        if (recv(socket_desc, server_message, sizeof(server_message), 0) == 0) {
            break;
        }

        poll(fds, 2, -1);
        // Citirea mesajelor trimise de catre clineti
        if (fds[0].revents & POLLIN) {
            memset(client_message, '\0', sizeof(client_message));
            fgets(client_message, sizeof(client_message), stdin);

            // In cazul comenzii exit, se trimite catre server si se deconecteaza
            if (strcmp(client_message, "exit\n") == 0) {
                send(socket_desc, client_message, strlen(client_message), 0);
                break;
            }

            // Daca nu s-a primit comanda exit, am primit subscribe/unsubscribe
            string aux = client_message;
            char action[32];
            char topic_name[32];    
            int SF;

            // Parsez comanda
            sscanf(aux.c_str(), "%s %s %d", action, topic_name, &SF);

            // Verific ce comanda a dat clientul : subscribe / unsubscribe

            // Pentru fiecare, se trimite toata comanda data de client la server
            if (strcmp(action, "subscribe") == 0) {
                printf("Subscribed to topic.\n");
                send(socket_desc, client_message, strlen(client_message), 0);
            } else if (strcmp(action, "unsubscribe") == 0) {
                printf("Unsubscribed from topic.\n");
                send(socket_desc, client_message, strlen(client_message), 0);
            } else if (send(socket_desc, client_message, strlen(client_message), 0) < 0){
                throw runtime_error("[client] Eroare la trimiterea mesajului");
            }
        }

        // Verific daca clientul a primit vreun mesaj de la server
        if (fds[1].revents & POLLIN) {
            memset(server_message, '\0', sizeof(server_message));
            if (recv(socket_desc, server_message, sizeof(server_message), 0) < 0){
                throw runtime_error("[client] Eroare la primirea mesajului de la server");
            }
            // Cazul in care serverul ii indica clientului faptul ca este deja
            // conectat
            if (strcmp(server_message, "You are already connected.") == 0) {
                // Oprirea clientului
                break;
            }
            // Cazul in care serverul a fost oprit
            if (strcmp(server_message, "Exit server") == 0) {
                // Oprirea clientului
                break;
            }
        }
    }

    // Inchid socketul TCP
    close(socket_desc);

    return 0;

}