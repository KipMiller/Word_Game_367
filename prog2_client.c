/* prog2_client.c - code for example client program that uses TCP */

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

/*------------------------------------------------------------------------
* Program: prog2_client
*
* Authors: Chris Miller and Michael Montgomery
*
* Purpose: Connects to the server to play a game similar to scrabble
*
* Syntax: ./prog2_client server_address server_port
*
* server_address - name of a computer on which server is executing
* server_port    - protocol port number server is using
*
*------------------------------------------------------------------------
*/

int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1000]; /* buffer for data from the server */
        
	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
    sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port >= 1024 && port <= 65535) /* test for legal value */
		sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		fprintf(stderr, "Use ports only between 1024 and 65535.\n");
		exit(EXIT_FAILURE);
	}
	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}
	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}
	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}
	/* Connect the socket to the specified server. You have to pass correct parameters to the connect function.*/
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}
	
	// The client receives this information once when they connect to the server 
	//=============================================================
	char player;// A character indicating whether the client is player '1' or '2' 
	n = recv(sd, &player, sizeof(player), 0);
	if(n < 0){
		perror("error: server closed\n");
		exit(EXIT_FAILURE);
	}
	if(player == '1'){
		printf("You are Player %c... the game will begin when Player 2 joins...\n", player);
	} else {
		printf("You are Player %c...\n", player);
	}
	uint8_t letters;// the number of letters on the board
	n = recv(sd, &letters, sizeof(letters), 0);
	if(n < 0){
		perror("error: server closed\n");
		exit(EXIT_FAILURE);
	}
	printf("Board size: %d \n", letters);

	uint8_t seconds;// the number of seconds per turn
	n = recv(sd, &seconds, sizeof(seconds), 0);
	if(n < 0){
		perror("error: server closed\n");
		exit(EXIT_FAILURE);
	}    
	printf("Seconds per turn: %d \n", seconds);
	//=============================================================
	
        //Start of Game Round
	while(1){// the actual game loop

        //Recieve P1 Score
        uint8_t player1Score;
        n = recv(sd, &player1Score, sizeof(player1Score), 0);
        if(n < 0){
            perror("error: server closed\n");
            exit(EXIT_FAILURE);
        }	   
        
        //Recieve P2 Score
        uint8_t player2Score; 
        n = recv(sd, &player2Score, sizeof(player2Score), 0);
        if(n < 0){
            perror("error: server closed\n");
            exit(EXIT_FAILURE);
        }	            
        
        //Recieve Round num
        uint8_t round; 
        n = recv(sd, &round, sizeof(round), 0);// receive the current round number
        if(n < 0){
            perror("error: server closed\n");
            exit(EXIT_FAILURE);
        }	
            
	    //Check Base Case
	    //Player 1 Loses
        if(player2Score == 3){
            if(player == '2'){
        		printf("You won!\n\n");
        		close(sd);
        		exit(EXIT_SUCCESS);
	        }else{
        		printf("You lost!\n\n");
        		close(sd);
        		exit(EXIT_SUCCESS);
	        }
	    //Player 2 Loses
	    }else if(player1Score == 3){
            if(player == '1'){
                printf("You won!\n\n");
        		close(sd);
    		    exit(EXIT_SUCCESS);
            }else{
        		printf("You lost!\n\n");
        		close(sd);
        		exit(EXIT_SUCCESS);
            }
	    }
        // print the current round number
	    printf("\nRound: %d...\n", round);
        
        //Print Scores
        if(player == '1'){// print the scores with correct order (player - opponent)
            printf("Score is %d-%d \n", player1Score, player2Score);// '1' score - '2' score
        } else {
            printf("Score is %d-%d \n", player2Score, player1Score);// '2' score - '1' score
        }

        //Recieve Board
        char board[letters];
        n = recv(sd, &board, sizeof(board), 0);// receive the board
        if(n < 0){
            perror("error: server closed\n");
            exit(EXIT_FAILURE);
        }	
	
        //Print Board
        printf("Board: ");
        for(int i = 0; i < letters; i++){
            printf("%c ", board[i]);// print the board with spaces between each letter
        }
        printf("\n");
        
        char yourTurn;// either Y or N
        while(1){
            //Recieve Y or N
            n = recv(sd, &yourTurn, sizeof(yourTurn), 0);
            if(n < 0){
                perror("error: server closed\n");
                exit(EXIT_FAILURE);
            }
            
            if(yourTurn == 'Y'){  
                printf("Your turn, enter word: ");
                
                //Get User Guess
                char guess[letters];
                fgets(guess, sizeof(guess), stdin);// get the guess from stdin 
                for(int i = 0; i < strlen(guess); i++){
                    guess[i] = tolower(guess[i]);// convert the client's guess to lowercase 
                }
                guess[strlen(guess)-1] = '\0';//need to get rid of newline character
                
                //Send length of guess, then the guess to the server
                uint8_t length  = strlen(guess);
                send(sd, &length, sizeof(length), 0);
                send(sd, &guess, sizeof(guess), 0);

                //Recieve Status of guess (valid or invalid)
                uint8_t status;
                n = recv(sd, &status, sizeof(status), 0);// receive the status (1 for correct 0 for incorrect)
                if(n < 0){
                    perror("error: server closed\n");
                    exit(EXIT_FAILURE);
                }
                
                //Check Status
                if(status == 1){
                        printf("Valid word!\n");
                }else {
                    printf("Invalid word!\n");
                    break;
                }
            }else if (yourTurn == 'N'){// if not your turn, wait until the opponent has entered a word
                printf("Please wait for opponent to enter word...\n");
                uint8_t status;
                n = recv(sd, &status, sizeof(status), 0);
                if(n < 0){
                    perror("error: server closed\n");
                    exit(EXIT_FAILURE);
                }
                
                //If opponent guessed was invalid, reset the round, else print his guess
                if(status == 0){
	                printf("Opponent lost the round!\n");
                    break;
                }else{
                    n = recv(sd, &buf, sizeof(buf), 0);
                    if(n < 0){
                        perror("error: server closed\n");
                        exit(EXIT_FAILURE);
                    }
                    printf("Opponent entered \"%s\"\n", buf);
                }
            } 
        }
    }
	close(sd);

	exit(EXIT_SUCCESS);
}

