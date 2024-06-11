/*
 * socket demonstrations:
 * This is the server side of an "internet domain" socket connection, for
 * communicating over the network.
 *
 * In this case we are willing to wait for chatter from the client
 * _or_ for a new connection.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#define BUFSIZE 50

#ifndef PORT
    #define PORT 30100
#endif

# define SECONDS 10

struct client {
    // file descriptor of socket
    int fd;
    // ip address
    struct in_addr ipaddr;
    // next thing in the linked list
    struct client *next;
    // next thing in the match linked list
    struct client *match_next;
    //name
    char name[BUFSIZE];
    // In battle
    int in_battle;
    // Current opp
    struct client *current_opp;
    // Text Buff
    char buffer[BUFSIZE];
    // Used to keep track how many are in the buffer
    int inbuf;
    // Hp of the player
    int hp;
    // Number of the powermove that each player have.
    int power_moves;
    // Whether or not its their turn
    int turn;
    // Whether or not they are saying anything
    int say;
    // how many times they have won
    int score;
};


// adds a client at the head of the linked list
static struct client *addclient(struct client *top, int fd, struct in_addr addr);
// removes a client with socket fd from the linked list
static struct client *removeclient(struct client *top, int fd);
// broadcasts a message to everyone in the linked list, they left it unfinished lmao
static void broadcast(struct client *top, char *s, int size, int no_send);

// Matches the first two available players together
int match(struct client **match_head);
// Handles the battling between two players who are currently matched
int battle_handler(struct client *play);
// Handles the say command when a player uses s
int say_handler(struct client *p, struct client **head, fd_set *allset);
// Writes the battle stats to a player


// add a client to the match linked list
void addmatchclient(struct client *p, struct client **head);
// remove a client from the match linked list
void removematchclient(struct client*p, struct client **head);
// handles disconnects for clients outside of battle
void disconnect_handler(struct client *p, struct client **head, fd_set *allset);
// handles disconnects for clients in battle
void disconnect_battle_handler(struct client *p, struct client **head, fd_set *allset);
// Writes battle stats
void write_battle_stats(struct client *player);
// writes players and their scores
void print_player_list(struct client* head, struct client* target);

// bind listener socket
int bindandlisten(void);

// A helper function that writes a string to the desired file descriptor.
void write_help(int fd, char *content);

/* The things that we hsould do:
1. naem
2. login
3. Shove ppl into battle

1. put into client list
2. fake until input a name
3. 

*/

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    struct client *match_head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    // struct timeval tv;
    fd_set allset;
    fd_set rset;

    srand(time(NULL));
    int i;


    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == 0) {
            printf("No response from clients in %d seconds\n", SECONDS);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }
        
        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            // ask them for a name
            write_help(clientfd, "What is your name?\n");
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
        }
        // Loop through all file descriptors and check if it is in select ready.
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        // check if name has been initialized (login)
                        if (p->name[0] == '\0') {
                            // read their input
                            int read_bytes = read(p->fd, p->buffer + p->inbuf, BUFSIZE);
                            if (read_bytes <= 0 ) {
                                disconnect_handler(p, &head, &allset);
                                break;
                            }
                            // Check if we have read up until a new line, replace the \n with a null terminator (same with \r if it exists)
                            p->inbuf += read_bytes;
                            if(p->buffer[p->inbuf-1] == '\n' || p->inbuf > BUFSIZE){
                                if(p->inbuf > BUFSIZE){
                                    p->inbuf = BUFSIZE;
                                }
                                p->buffer[p->inbuf-1] = '\0';
                                if(p->buffer[p->inbuf-2] == '\r'){p->buffer[p->inbuf-2] = '\0';}
                                // put their input as the name
                                strcpy(p->name, p->buffer);
                                // reset name buffer
                                p->inbuf = 0;
                                // initialize some battle and score attributes to 0
                                p->in_battle = 0;
                                p->score = 0;
                                p->say = 0;
                                
                                // broadcast the entry
                                char player_msg[2 * BUFSIZE];
                                sprintf(player_msg,  "**%s has entered the arena**\n", p->buffer);
                                broadcast(head, player_msg, strlen(player_msg), p->fd);
                                player_msg[0] = '\0';
                                
                                // Writes "Welcome, USERNAME! Awaiting opponent..." to the user who just joined
                                char welcome_msg[2 *  BUFSIZE];
                                sprintf(welcome_msg, "Welcome, %s!\n", p->buffer);
                                write_help(p->fd, welcome_msg);
                                welcome_msg[0] = '\0';
                                write_help(p->fd, "(l)obby - See the players in the lobby and their score\n(m)atch - Enter the queue and try to match with another player\n");
                            }
                        }
                        else if(p->in_battle){ // handle their command if they're in battle
                            if(p->turn){ // if its their turn, process it
                                if (p->say) {
                                    // call the function to handle their input if they say something
                                    say_handler(p, &head, &allset);
                                    break;
                                }
                                else {
                                    // read their input and process it if its their turn
                                    char move[1];
                                    int read_bytes = read(p->fd, move, 1);
                                    if (read_bytes <= 0) {
                                        disconnect_battle_handler(p, &head, &allset);
                                        break;
                                    }
                                    // write to them if their input is s
                                    if(move[0] == 's'){
                                        p->say = 1;
                                        write_help(p->fd, "\nSpeak: ");
                                        break;
                                    }
                                    // perform actions if attack
                                    else if(move[0] == 'a'){
                                        int attack = (rand() % 5) + 2; // Determine damage
                                        char resultmessage[2*BUFSIZE];
                                        char recievemessge[2*BUFSIZE];
                                        
                                        p->current_opp->hp = p->current_opp->hp - attack; // Decrease Hp
                                        
                                        sprintf(resultmessage, "\nYou hit %s for %d damage!\n", p->current_opp->name, attack); // Write how much damage you did
                                        sprintf(recievemessge, "\n%s hits you for %d damage!\n", p->name, attack); // Write how much damage you took
                                        write_help(p->current_opp->fd, recievemessge);
                                        write_help(p->fd, resultmessage);
                                    }
                                    // perform power move
                                    else if(move[0] == 'p'){
                                        if(p->power_moves == 0){
                                            // do nothing if they have no power moves
                                            break;
                                        }
                                        p->power_moves -= 1;
                                        int land = rand()%2;
                                        if(land){
                                            // power move hits
                                            int attack =  3 *((rand() % 5 )+2);
                                            char resultmessage[2*BUFSIZE];
                                            char recievemessage[2*BUFSIZE];

                                            p->current_opp->hp = p->current_opp->hp-attack;
                                            
                                            sprintf(resultmessage, "\nYou hit %s for %d damage!\n", p->current_opp->name, attack);
                                            sprintf(recievemessage, "\n%s powermoves you for %d damage!\n", p->name, attack); // Write how much damage you took
                                            
                                            write_help(p->current_opp->fd, recievemessage);
                                            write_help(p->fd, resultmessage);
                                        }else{
                                            //If the powermove miss.
                                            char receivemessage[2*BUFSIZE];
                                            sprintf(receivemessage, "\n%s missed!\n", p->name);
                                            write_help(p->current_opp->fd, receivemessage);
                                            write_help(p->fd, "\nYour power move did not land successfully...\n");
                                        }
                                    }
                                    else {
                                        // if their command is invalid do nothing
                                        break;
                                    }
                                    p->turn = 0;
                                    p->current_opp->turn = 1;

                                    if (p->current_opp->hp <= 0) {
                                        // print victory and defeat messages
                                        char victors_message[4*BUFSIZE];
                                        char losers_message[4*BUFSIZE];
                                        sprintf(victors_message, "You have claimed victory with ease against the weak and pitiful %s! You take their belongings and their head like a true victor.\n", p->current_opp->name);
                                        sprintf(losers_message, "You have lost miserably against the undefeatable %s!!!! Muahahahahahhahah!!!!\n", p->name );
                                        
                                        // writing to the winner 
                                        write_help(p->fd, victors_message);
                                        write_help(p->fd, "(l)obby - See the players in the lobby and their score\n(m)atch - Enter the queue and try to match with another player\n");
                                        // Writing to the loser
                                        write_help(p->current_opp->fd, losers_message);
                                        write_help(p->current_opp->fd, "(l)obby - See the players in the lobby and their score\n(m)atch - Enter the queue and try to match with another player\n");
                                        
                                        p->score += 1;
                                        
                                        // move both players out of battle
                                        p->in_battle = 0;
                                        p->current_opp->in_battle = 0;
                                    }
                                    else {
                                        // write the battle stats if they didn't finish yet
                                        write_battle_stats(p);
                                        write_battle_stats(p->current_opp);
                                    }
                                }
                            }
                            else {
                                // discard it if it's not their turn, they will only send one byte at a time
                                char testbyte[1];
                                int result = read(p -> fd, testbyte, 1);
                                if (result <= 0) {
                                    disconnect_battle_handler(p, &head, &allset);
                                    break;
                                }
                            }
                        }
                        else {
                            // due to -icanon they will only send one byte at a time, check to see if it's m or l and otherwise discard
                            char testbyte[1];
                            int result = read(p -> fd, testbyte, 1);
                            if (result <= 0) {
                                disconnect_handler(p, &head, &allset);
                                removematchclient(p, &match_head);
                            }
                            else if (testbyte[0] == 'm') {
                                write_help(p->fd, "\nAwaiting opponent...\n");
                                addmatchclient(p, &match_head);
                                match(&match_head);
                            }
                            else if (testbyte[0] == 'l') {
                                print_player_list(head, p);
                            }
                            break;
                        }
                    }
                } // Looping through the list of players
            }
        } // End of FOR loop
    }// End of WHILE LOOP
    return 0;  
}  

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->name[0] = '\0';
    top = p;
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;
    // // add loop to deep clean through opponent pointers
    for (p=&top; *p; p = &(*p)->next){
        if ((*p) -> current_opp){
            if((*p)->current_opp->fd == fd){
                (*p)->current_opp = NULL;            
            }
        }
    }
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", 
                 fd);
    }
    return top;
}


static void broadcast(struct client *top, char *s, int size, int no_send) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if(p->fd != no_send){
            write_help(p->fd, s);
        }
    }
}

// our helpers

int match(struct client **match_head) {
    struct client* playerone;
    int player_one_change = 0;
    struct client* playertwo;
    struct client* p = *match_head;
    // there is no one avaliable to match currently
    if (p == NULL) {
        return 0;
    }
    
    while(p != NULL){
        if(p->in_battle == 0){
            if(!player_one_change){
                playerone = p;
                player_one_change = 1;
            }
            else{
                // Match found, assign current opps, put them in battle, assign their hp, assign their power moves
                if(p->current_opp != playerone || playerone->current_opp != p){
                    playertwo = p;
                    playertwo->current_opp = playerone;
                    playerone->current_opp = playertwo;
                    playerone->in_battle = 1;
                    playertwo->in_battle = 1;
                    removematchclient(playerone, match_head);
                    removematchclient(playertwo, match_head);   
                    //assign hp and power move (To be implemented)
                    playerone->hp = (rand() % 11) + 20;
                    playertwo->hp = (rand() % 11) + 20;
                    playerone->power_moves = (rand() % 3) + 1;
                    playertwo->power_moves = (rand() % 3) + 1;

                    // Deciding who goes first
                    int heads = rand() % 2;
                    playerone->turn = heads;
                    playertwo->turn = 1-heads;
                    //write match details to each person 
                    char startmsg1[2*BUFSIZE]; // Writing to player one
                    sprintf(startmsg1, "You engage %s!\n", playertwo->name);
                    write_help(playerone->fd, startmsg1);
                    startmsg1[0] = '\0';
                    write_battle_stats(playerone);
                    
                    
                    char startmsg2[2*BUFSIZE];// Writing to player two
                    sprintf(startmsg2, "You engage %s!\n", playerone->name);
                    write_help(playertwo->fd, startmsg2);
                    startmsg2[0] = '\0';
                    write_battle_stats(playertwo);
                }
            }
        }
        p = p->match_next;
    }
    return 0;
}

void addmatchclient(struct client *p, struct client **head){
    struct client *curr = *head;
    if (*head == NULL) {
        *head = p;
        return;
    }
    while (curr->match_next) {
        curr = curr->match_next;
    }
    curr->match_next = p;
}

void removematchclient(struct client*p, struct client **head){
    if (*head == NULL) {
        return;
    }
    struct client * curr = *head;
    if (curr == p) {
        *head = curr->match_next;
        curr->match_next = NULL;
        return;
    }
    while (curr->match_next) {
        if (curr->match_next == p) {
            struct client *t = curr->match_next->match_next;
            curr->match_next->match_next = NULL;
            curr->match_next = t;
            return;
        }
    }
}

int say_handler(struct client *p, struct client **head, fd_set *allset) {
    // read their input
    int read_bytes = read(p->fd, p->buffer + p->inbuf, BUFSIZE);
    if (read_bytes <= 0) {
        disconnect_battle_handler(p, head, allset);
    }
    // Check if we have read up until a new line, replace the \n with a null terminator (same with \r if it exists)
    p->inbuf += read_bytes;
    if(p->buffer[p->inbuf-1] == '\n' || p->inbuf > BUFSIZE){
        if(p->inbuf > BUFSIZE){
            p->inbuf = BUFSIZE;
        }
        p->buffer[p->inbuf-1] = '\0';
        if(p->buffer[p->inbuf-2] == '\r'){p->buffer[p->inbuf-2] = '\0';}
        // write what they said to the other person
        // currently writes [name] says: [msg] to both people, can change later
        char player_msg[3 *  BUFSIZE];
        sprintf(player_msg, "%s takes a break to tell you:\n%s \n\n", p->name, p->buffer);
        write_help(p->current_opp->fd, player_msg);
        sprintf(player_msg, "You speak: %s\n\n", p->buffer);
        write_help(p->fd, player_msg);
        player_msg[0] = '\0';
        // rewrite the battle stats
        write_battle_stats(p);
        write_battle_stats(p->current_opp);
        // reset name buffer
        p->inbuf = 0;
        // stop saying
        p->say = 0;
    }
    return 0;
}

/* Writes your hp, powermoves and enemy hp. Parameter it the 
 * client struct of the current player
*/
void write_battle_stats(struct client *player){
    char your_hp[2*BUFSIZE];
    char your_power[2*BUFSIZE];
    char enemy_hp[2*BUFSIZE];
    char not_your_turn[2*BUFSIZE];

    sprintf(your_hp, "Your hitpoints: %d\n", player->hp);
    sprintf(your_power, "Your powermoves: %d\n\n", player->power_moves);
    sprintf(enemy_hp, "%s's hitpoints: %d\n", player->current_opp->name, player->current_opp->hp);
    sprintf(not_your_turn, "Waiting for %s to strike...\n", player->current_opp->name);

    write_help(player->fd, your_hp);
    write_help(player->fd, your_power);
    write_help(player->fd, enemy_hp);
    
    // write commands
    if(player->turn){
        if(player->power_moves != 0){
            write_help(player->fd, "\n(a)ttack\n(p)owermove\n(s)peak something\n");
        }
        else{
            write_help(player->fd, "\n(a)ttack\n(s)peak something\n");
        }
    }else{
        write_help(player->fd, not_your_turn);
    }
    write_help(player->fd, "--------------------\n");
    return;
}

// Writes something with error checking and stuff
void write_help(int fd, char *content){
    int message_len = strlen(content);
    char* message_start = &content[0];
        int ret;
        while (message_len != 0 && (ret = write(fd, message_start, message_len))){
            if(ret == -1){
                perror("write");
            }
            message_len -= ret;
            message_start += ret;
        }
    return;   
}

//print the list of players to the targeted player.
void print_player_list(struct client* head, struct client* target ){
    struct client* curr = head;
    char line[2*BUFSIZE];
    sprintf(line, "\nHere is the list of players: \n");
    write_help(target->fd, line);
    int number = 0;
    line[0] = '\0';
    
    while(curr){
        sprintf(line, "%s : %d points\n", curr->name, curr->score);
        write_help(target->fd, line);
        line[0] = '\0';
        curr = curr->next;
        number ++;
    }
    write_help(target->fd, "\n(l)obby - See the players in the lobby and their score\n(m)atch - Enter the queue and try to match with another player\n");
    return;
}


void disconnect_battle_handler(struct client *p, struct client **head, fd_set *allset) {
    char disconnect_msg[2 * BUFSIZE];
    sprintf(disconnect_msg,  "**%s has disconnected**\n", p->name);
    broadcast(*head, disconnect_msg, strlen(disconnect_msg), p->fd);
    int tmp_fd = p->fd;
    char victors_message[4*BUFSIZE];
    sprintf(victors_message, "Your opponent fled in fear from your enormous power!\n");
    write_help(p->current_opp->fd, victors_message);
    write_help(p->current_opp->fd, "\n(l)obby - See the players in the lobby and their score\n(m)atch - Enter the queue and try to match with another player\n");
    p->current_opp->in_battle = 0;
    p->current_opp->say = 0;
    p->current_opp->score += 1;
    *head = removeclient(*head, p->fd);
    FD_CLR(tmp_fd, allset);
    close(tmp_fd);
}

void disconnect_handler(struct client *p, struct client **head, fd_set *allset) {
    char disconnect_msg[2 * BUFSIZE];
    sprintf(disconnect_msg,  "**%s has disconnected**\n", p->name);
    broadcast(*head, disconnect_msg, strlen(disconnect_msg), p->fd);
    int tmp_fd = p->fd;
    *head = removeclient(*head, p->fd);
    FD_CLR(tmp_fd, allset);
    close(tmp_fd);
}