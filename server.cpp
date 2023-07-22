#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <iostream>
#include <vector>
#include <fcntl.h>

using namespace std;

struct Client {
    int socket;
    sockaddr_in addr;
    string id;
};

struct Topic {
    string name;
    vector<string> clients;
};

int main(int argc, char *argv[]) {

    //Verific numarul de argumente
    if (argc != 2) {
        throw runtime_error("[server] Eroare la argumente");
    }

    int client_sock;
    socklen_t client_size;

    char server_message[1501], client_message[1501];
    struct sockaddr_in server_addr, client_addr, udp_addr;

    memset(server_message, 0, sizeof(server_message));
    memset(client_message, 0, sizeof(client_message));
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    memset(&udp_addr, 0, sizeof(udp_addr));

    // Crearea socketului TCP
    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        throw runtime_error("[server] Eroare la crearea socketului");
    }

    // Crearea socketului UDP
    int udp_sock;
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        throw runtime_error("[server] Eroare la crearea socketului UDP");
    }

    // Leg portul socketilor de TCP si UDP la portul serverului
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(stoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        throw runtime_error("[server] Nu s-a putut lega portul TCP");
    }

    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(stoi(argv[1]));
    udp_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock, (struct sockaddr *) &udp_addr, sizeof(udp_addr)) < 0) {
        throw runtime_error("[server] Nu s-a putut lega portul UDP");
    }

    if (listen(socket_desc, 1) < 0) {
        throw runtime_error("[server] Nu s-a putut asculta socketul TCP");
    }

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);

    struct pollfd fds[1024];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = socket_desc;
    fds[1].events = POLLIN;
    fds[2].fd = udp_sock;
    fds[2].events = POLLIN;

    int nfds = 3;

    vector<Client> clients;
    vector<Topic> topics;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    while (1) {
        int activity = poll(fds, nfds, -1);

        if (activity < 0) {
            throw runtime_error("[server] Eroare poll");
        }

        // Citirea de la STDIN
        if (fds[0].revents & POLLIN) {
            fgets(server_message, sizeof(server_message), stdin);
            
            // Daca s-a primit mesajul exit
            if (strcmp(server_message, "exit\n") == 0) {
                // Oprirea serverului
                break;
            }
        }

        // Accepta un nou client TCP
        if (fds[1].revents & POLLIN) {
            client_size = sizeof(client_addr);
            client_sock = accept(socket_desc, (struct sockaddr *) &client_addr,
                &client_size);
            if (client_sock < 0) {
                throw runtime_error("[server] Nu s-a putut accepta conexiunea");
            }

            char client_id[50];
            memset(client_id, 0, sizeof(client_id));
            if (recv(client_sock, client_id, sizeof(client_id), 0) > 0) {
                bool client_already_connected = false;

                // Verific daca clientul este deja conectat la server
                for (uint i = 0; i < clients.size(); ++i) {
                    if (clients[i].id == string(client_id)) {
                        client_already_connected = true;
                        break;
                    }
                }

                if (client_already_connected) {
                    printf("Client %s already connected.\n", client_id);

                    const char *error_message = "You are already connected.";
                    // Trimit clientului un mesaj, urmand sa inchid clientul
                    // la primirea acestui mesaj
                    send(client_sock, error_message, strlen(error_message), 0);

                    close(client_sock);
                } else {
                    // Cazul in care un client nu este deja conectat
                    printf("New client %s connected from %s:%i.\n",
                        client_id, inet_ntoa(client_addr.sin_addr),
                        ntohs(client_addr.sin_port));

                    // Creez un client nou pe care il adaug in vectorul de 
                    // clienti.
                    Client new_client;
                    new_client.socket = client_sock;
                    new_client.addr = client_addr;
                    new_client.id = string(client_id);
                    clients.push_back(new_client);

                    // Adaugarea unui nou client la poll fds
                    fds[nfds].fd = client_sock;
                    fds[nfds].events = POLLIN;
                    nfds++;
                    continue;
                }
            }
        }

        // Primirea mesajelor de la clientii TCP
        for (int i = 3; i < nfds; ++i) {
            if (fds[i].revents & POLLIN) {
                int recv_size = recv(fds[i].fd, client_message, sizeof(client_message), 0);
                if (recv_size > 0) {
                    // In cazul in care am primit din partea unui client
                    // mesajul "exit", il elimin din lista de clienti conectati
                    // la momentul de timp, si afisez mesajul de deconectare.
                    if (strcmp(client_message, "exit\n") == 0) {
                        for (uint j = 0; j < clients.size(); j++) {
                            if (clients[j].socket == fds[i].fd) {
                                printf("Client %s disconnected.\n", clients[j].id.c_str());
                                close(clients[j].socket);
                                clients.erase(clients.begin() + j);
                                break;
                            }
                        }
                        // Elimina clientii deconectati din poll fds
                        for (int j = i; j < nfds - 1; ++j) {
                            fds[j] = fds[j + 1];
                        }
                        nfds--;
                        i--;
                    } else {
                        // Cazul in care clientul a dat una dintre comenzile
                        // de subscribe sau unsubscribe
                        string aux = client_message;
                        char action[32];
                        char topic_name[32];    
                        int SF;

                        // Parsez comanda de subscribe / unsubscribe
                        sscanf(aux.c_str(), "%s %s %d", action, topic_name, &SF);

                        if (strcmp(action, "subscribe") == 0) {
                            // Verific daca topicul exista deja
                            for (uint i = 0; i < topics.size(); i++) {
                                if (topics[i].name == topic_name) {
                                    // Cauta clientul care a dat subscribe
                                    for (uint j = 0; j < clients.size(); j++) {
                                        if (clients[j].socket == fds[i].fd) {
                                            // Verific daca clientul nu este deja abonat la topicul respectiv
                                            for (uint k = 0; k < topics[i].clients.size(); k++) {
                                                if (topics[i].clients[k] != clients[j].id) {
                                                    // Adaug clientul in lista topicului respectiv
                                                    topics[i].clients.push_back(clients[j].id);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(action, "unsubscribe") == 0) {
                            // Cauta topicul
                            for (uint i = 0; i < topics.size(); i++) {
                                if (topics[i].name == topic_name) {
                                    for (uint j = 0; j < clients.size(); j++) {
                                        if (clients[j].socket == fds[i].fd) {
                                            // Salvez pozitia clientului in vector
                                            int pos = 0;
                                            for (uint k = 0; k < topics[i].clients.size(); k++) {
                                                if (topics[i].clients[k] != clients[j].id) {
                                                    pos += 1;
                                                }
                                                // Elimin clientul din lista de clienti a topicului respectiv
                                                if (!pos) {
                                                    topics[i].clients.erase(topics[i].clients.begin());
                                                } else {
                                                    topics[i].clients.erase(topics[i].clients.begin() + pos);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    memset(client_message, 0, sizeof(client_message));
                }
            }
        }

        // Primesc mesaje de la clientii UDP
        if (fds[2].revents & POLLIN) {
            client_size = sizeof(client_addr);
            if (recvfrom(udp_sock, client_message, sizeof(client_message), 0,
                (struct sockaddr *) &client_addr, & client_size) > 0) {
                memset(client_message, 0, sizeof(client_message));
            }
        }
    }

    // Inchid socketii clientilor
    for (uint i = 0; i < clients.size(); i++) {
        close(clients[i].socket);
    }

    // Inchid socketul TCP si socketul UPD
    close(socket_desc);
    close(udp_sock);

    return 0;
}

           
