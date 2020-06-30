// Prog2_server.c 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include "trie.c"

/*----------------------------------------------------------------------------------------------
* Program: prog2_server
*
* Authors: Chris Miller and Michael Montgomery
*
* Purpose: Server that when run can accept multiple clients to play a game similar to scrabble
*
* Syntax: ./prog2_server server_port boardSize turnSeconds dictionaryPath
*
* 
* server_port    - protocol port number server is using
* boardSize      - the number of characters to appear on the board
* turnSeconds    - the number of seconds designated for a turn 
* dicionaryPath  - the path to a text file containing a valid dictionary of words
* 
*------------------------------------------------------------------------------------------------ 
*/

#define QLEN 6 /* size of request queue */

// Function that will fill a length K board with random characters 
// selected from the alphabet (duplicate characters are allowed).
// If no vowel has been added by the final letter, a random vowel 
// will be manually inserted.
void generateBoard(char* board, int k){
	char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
	char vowels[] = "aeiou";
	int vowelFlag = 0;// used to tell if a vowel has been placed or not
	
	srand((unsigned int) time(0));// set the random aspect
	
	for(int i = 0; i < k; i++){
		if(i == k-1 && vowelFlag != 1){
			char last = vowels[rand() % 5];
			//printf("Manually inserting a vowel %c.\n", last);// print to make sure its working
			board[i] = last;
		} else {
			board[i] = alphabet[rand() % 26];//get a random char from the alphabet
			for(int j = 0; j < 5; j++){// number of vowels 
				if(board[i] == vowels[j])// if there is a single vowel in the board 
					vowelFlag = 1;// set the flag 
			}
		}
		printf("%c ", board[i]);// print each element of board with a space after
	}
	printf("\n");
	board[k] = '\0';//make sure the board ends in a null 
}

// Function to check whether or not the user's guess has the correct number of letters
// and makes sure each letter is on the board. 
int checkLetters(char* board, char* guess){
    char letters[sizeof(board)];// make a temp board to modify
    strcpy(letters, board);//copy board over to the temp (letters)
    int flag = 0;// flag to signify whether or not a letter is on the board
    
    for(int i = 0; i < strlen(guess); i++){// loop through every letter in the client's guess
        for(int j = 0; j < sizeof(letters); j++){// loop through every letter of the board
            if(guess[i] == letters[j]){//if the user's letter matches a letter on the board
                letters[j] = '_';// take that letter off the board (for future iterations)
                flag = 1;// mark this letter OK
            }
        }
        if(flag == 0){//None of the letters in user guess matches the board
            printf("\nLetter in guess was NOT on the board. \n");
            return 0;// REJECT
        }
        flag = 0;//reset the flag for the next letter
    }
    return 1;// accept
}

// Function to check the validity of the client's guess, based on three conditions. 
// 1) The word must be a valid word in the dictionary 
// 2) Must not have been used this round (in the usedWords trie)
// 3) The letters of the guess must be on the board (with no more than they appear on the board)
uint8_t checkGuess(char* board, struct TrieNode *usedWords, struct TrieNode *root, char* guess){
    
	if( (search(root, guess) == true) && (search(usedWords, guess) == false) && (checkLetters(board,guess) == 1) ){
		printf("'%s' is a valid word! \n", guess);// if it is in the dictionary and NOT in the used words trie
		insert(usedWords, guess);// insert each word into our trie
		uint8_t win = 1;
		return win; 
	} else {
		printf("'%s' is NOT a valid word! \n", guess);
		uint8_t loss = 0;
		return loss;
	}
}

// Main method for the server, usage: 
// argv[1] = port for server
// argv[2] = the board size 
// argv[3] = seconds per round 
// argv[4] = path to the dictionary
int main( int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	struct sockaddr_in cad; /* structure to hold client's address */
    socklen_t alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	char buf[1000]; /* buffer for string the server sends */
	int sd, sd2, sd3; /* socket descriptors */
	int port; /* protocol port number */
    char N = 'N';
	char Y = 'Y';
	int turn = 1;
    int n;

	// Scrabble game setup  ------------------------------------------
	char *fileName = argv[4];
	FILE *fp = fopen(fileName, "r");// open the file specified by argv[4]
	if(fp == NULL){
		printf("Cannot open file: %s", fileName);
		exit(EXIT_FAILURE);
	}
	char line[50];// used to read line by line from the text file	
	struct TrieNode *root = getNode();// make the trie to hold the dictionary
	while(fgets(line, sizeof(line), fp) != NULL){
		char *inserting = line; 
		inserting[strlen(line)-1] = '\0';//need to get rid of newline character or it crashes
		insert(root, inserting);// insert each word into our trie
	}
	fclose(fp);// close the file when done loading dictionary into trie

	uint8_t round = 1;// a game consists of 3-5 rounds
	
	// number of each player's wins
	uint8_t player1Win = 0;
	uint8_t player2Win = 0;
    uint8_t timedOut = 0;// sent if either player takes too long on their turn
	
	// set the board size and number of seconds per turn
	uint8_t boardSize = 0;
	boardSize = atoi(argv[2]);
	uint8_t turnSec = 0; 
	turnSec = atoi(argv[3]);
	// --------------------------------------------------------------------------
       	
	if( argc != 5 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server server_port board_size seconds_per_round path_to_dictionary\n");
		exit(EXIT_FAILURE);
	}
	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	printf("Server Running...\n");
   
	// Set socket family to AF_INET
	sad.sin_family = AF_INET;
        
	//Set local IP address to listen to all IP addresses this server can assume. You can do it by using INADDR_ANY
    sad.sin_addr.s_addr = INADDR_ANY;
        
	port = atoi(argv[1]); /* convert argument to binary */
	if (port >= 1024 && port <= 65535) { /* test for illegal value */
        sad.sin_port = htons(port);// set port number. The data type is u_short
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		fprintf(stderr, "Use ports only between 1024 and 65535.\n");
		exit(EXIT_FAILURE);
	}
	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}
	/* Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named sd. */
        sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}
	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}
	/* Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
	if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}
	/* Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
	if (listen(sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

	char player;// either '1' or '2'
	while(1){//Parent server loop
        alen = sizeof(cad);
        
        if ( (sd2=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
            fprintf(stderr, "Error: Accept failed\n");
            exit(EXIT_FAILURE);
        } 
        player = '1';
         // This will be sent to the client once =================
        send(sd2, &player, sizeof(player), 0);// send the player character ('1' or '2')
        send(sd2, &boardSize, sizeof(boardSize), 0);// send how many characters are on the board
        send(sd2, &turnSec, sizeof(turnSec), 0);// send how many seconds per turn
        // ======================================================
        
        if ( (sd3=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
            fprintf(stderr, "Error: Accept failed\n");
            exit(EXIT_FAILURE);
        } 
        player = '2';
         // This will be sent to the client once =================
        send(sd3, &player, sizeof(player), 0);// send the player character ('1' or '2')
        send(sd3, &boardSize, sizeof(boardSize), 0);// send how many characters are on the board
        send(sd3, &turnSec, sizeof(turnSec), 0);// send how many seconds per turn
        // ======================================================
           
        int child_pid = fork();// fork the server so that multiple games can coincide
        
        if(child_pid < 0){
            perror("Error creating child process!\n");
            exit(1);
        }else if(child_pid == 0){
        
            printf("Starting game!\n");
            while(1){// actual game loop / round loop 
                //Round Start
                struct TrieNode *usedWords = getNode();// make the trie to hold the used words for the round	
                // sd2 = player 1 
                send(sd2, &player1Win, sizeof(player1Win), 0);//send player 1's score
                send(sd2, &player2Win, sizeof(player2Win), 0);//send player 2's score
                send(sd2, &round, sizeof(round), 0);// send the current round
                //sd3 = player 2 
                send(sd3, &player1Win, sizeof(player1Win), 0);//send player 1's score
                send(sd3, &player2Win, sizeof(player2Win), 0);//send player 2's score
                send(sd3, &round, sizeof(round), 0);// send the current round
    
    		    //Server base case
                if(player1Win == 3 || player2Win == 3){
                    free(usedWords);// free up the usedWords trie to prevent memory leak
					if(player1Win == 3){
                        printf("Player 1 Wins!\n");
                        close(sd2);
                        close(sd3);
                        exit(EXIT_SUCCESS);
                    }else{
                        printf("Player 2 Wins!\n");
                        close(sd2);
                        close(sd3);
                        exit(EXIT_SUCCESS);
                    }
                }
        
                char board[boardSize];// make a board element
                generateBoard(board, boardSize);// fill it with random letters
                send(sd2, &board, sizeof(board), 0);// send the randomly generated board
                send(sd3, &board, sizeof(board), 0);// send the randomly generated board
        
                turn = round;
                uint8_t strlength;
                //Turn Start
                while(1){//turn loop
                    if(turn % 2 == 1){//Player 1's turn
                        //send Y to player 1, N to player 2
                        send(sd2, &Y, sizeof(Y), 0);
                        send(sd3, &N, sizeof(N), 0);
                        
                        // Establish the timeout for the socket
                        struct timeval tv; 
                        tv.tv_sec = turnSec;
                        tv.tv_usec = 0;
                        setsockopt(sd2, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
                        
                        // receive the client's guess length
                        n = (recv(sd2, &strlength, sizeof(strlength), 0));
    
                        if(n == -1 && errno == EAGAIN){// if player 1 times out
                            printf("Player 1: TIMEOUT \n");
                            send(sd2, &timedOut, sizeof(timedOut), 0);// send 0 to both clients
                            send(sd3, &timedOut, sizeof(timedOut), 0);
                            player2Win++;
                            break;
                        } else if (n == 0){// if the client disconnected
							perror("error: client disconnected\n");
                            close(sd2);// close both sockets
							close(sd3);
							exit(EXIT_FAILURE);
						}
                        
                        // receive the client's guess
                        if(recv(sd2, &buf, sizeof(buf), 0) < 0){
                            perror("error: client disconnected\n");
                            exit(EXIT_FAILURE);
                        }
                        printf("\nUser Guess: '%s' Length: %d \n", buf, strlength);
                        uint8_t status = checkGuess(board, usedWords, root, buf);// determine whether or not the user guess is valid
             
                        if(status == 1){//valid guess 
                            send(sd2, &status, sizeof(status), 0);
                            send(sd3, &strlength, sizeof(strlength), 0);
                            send(sd3, &buf, sizeof(buf), 0);
                            
                        }else{//invalid guess
                            send(sd2, &status, sizeof(status), 0);
                            send(sd3, &status, sizeof(status), 0);
                            player2Win++;
                            break;
                        }
                        turn++;
                        
                    }else{//Player 2's turn
                        //Send N to player 1, Y to player 2
                        send(sd2, &N, sizeof(N), 0);
                        send(sd3, &Y, sizeof(Y), 0);
                    
                        // Establish the timeout for this socket
                        struct timeval tv; 
                        tv.tv_sec = turnSec;
                        tv.tv_usec = 0;
                        setsockopt(sd3, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
                    
                        // receive the client's guess length
                        n = (recv(sd3, &strlength, sizeof(strlength), 0));
    
                        if(n == -1 && errno == EAGAIN){// if player 2 times out 
                            printf("Player 2: TIMEOUT \n");
                            send(sd2, &timedOut, sizeof(timedOut), 0);// send 0 to both clients
                            send(sd3, &timedOut, sizeof(timedOut), 0);
                            player1Win++;
                            break;
                        } else if (n == 0){// if the client disconnected
							perror("error: client disconnected\n");
                            close(sd2);// close both sockets
							close(sd3);
							exit(EXIT_FAILURE);
						}
                        
                        // receive the client's guess
                        if(recv(sd3, &buf, sizeof(buf), 0) < 0){
                            perror("error: client disconnected\n");
							exit(EXIT_FAILURE);
                        }
                        printf("\nUser Guess: '%s' Length: %lu \n", buf, strlen(buf));
                        uint8_t status = checkGuess(board, usedWords, root, buf);// determine whether or not the user guess is valid
                        
                        if(status == 1){//valid guess 
                            send(sd3, &status, sizeof(status), 0);
                            send(sd2, &strlength, sizeof(strlength), 0);
                            send(sd2, &buf, sizeof(buf), 0);
                            
                        }else{//invalid guess
                            send(sd3, &status, sizeof(status), 0);
                            send(sd2, &status, sizeof(status), 0);
                            player1Win++;
                            break;
                        }
                        turn++;
                    }   
                }
                round++;
            }
        }
	}
	free(root);// free up the memory from the root trie (so there is no memory leak)
	return 0;
}
