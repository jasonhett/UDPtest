#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

#define ECHOMAX 50000     /* Longest string to echo */

void DieWithError(char *errorMessage);  /* External error handling function */
struct ACKPacket createACKPacket (int ack_type, int base);
int is_lost(float loss_rate);

struct segmentPacket {
    int type;
    int seq_no;
    int length;
    char data[512];
};
struct ACKPacket {
    int type;
    int ack_no;
};

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char echoBuffer[ECHOMAX];        /* Buffer for echo string */
    unsigned short echoServPort;     /* Server port */
    int recvMsgSize;                 /* Size of received message */
    int chunkSize;                   /* Size of chunks to send */
    float loss_rate = 0;                  /* Size of the window for packets to send */

    srand48(2345);

    if (argc < 3 || argc > 4)         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <UDP SERVER PORT> <CHUNK SIZE> [<LOSS RATE>]\n", argv[0]);
        exit(1);
    }

    echoServPort = atoi(argv[1]);  /* First arg:  local port */
    chunkSize = atoi(argv[2]);  /* Second arg:  size of chunks */
    if(argc == 4){
        loss_rate = atof(argv[3]);

    }


    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");

    char dataBuffer[8192];
    int base = -2;
    int seqNumber = 0;

    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliAddrLen = sizeof(echoClntAddr);

        /* struct for incoming datapacket */
        struct segmentPacket dataPacket;

        /* struct for outgoing ACK */
        struct ACKPacket ack;

        /* Block until receive message from a client */
        if ((recvMsgSize = recvfrom(sock, &dataPacket, sizeof(dataPacket), 0,
            (struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
            DieWithError("recvfrom() failed");

        /* Add random packet lose, if lost dont process */
        if(!is_lost(loss_rate)){
            /* If seq is zero start new data collection */
            if(dataPacket.seq_no == 0 && dataPacket.type == 1)
            {
                printf("Recieved Initial Packet from %s\n", inet_ntoa(echoClntAddr.sin_addr));
                memset(dataBuffer, 0, sizeof(dataBuffer));
                strcpy(dataBuffer, dataPacket.data);
                base = 0;
                ack = createACKPacket(2, base);
            } else if (dataPacket.seq_no == base + 1){
                /* else concatinate the data */
                printf("Recieved  Subseqent Packet #%d from %s\n", dataPacket.seq_no, inet_ntoa(echoClntAddr.sin_addr));
                strcat(dataBuffer, dataPacket.data);
                base++;
                ack = createACKPacket(2, base);
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1)
            {
                printf("Recieved Out of Sync Packet #%d from %s\n", dataPacket.seq_no, inet_ntoa(echoClntAddr.sin_addr));
                /* Resend ACK with old base */
                ack = createACKPacket(2, base);
            }

            if(dataPacket.type == 4){
                printf("Recieved Teardown Packet from %s\n", inet_ntoa(echoClntAddr.sin_addr));
                printf("\n MESSAGE RECIEVED\n %s\n\n", dataBuffer);
                memset(dataBuffer, 0, sizeof(dataBuffer));
                base = -1;
                ack = createACKPacket(8, base);
            }

            /* Send ACK for Packet Recieved */
            if(base >= 0){
                printf("Sending ACK #%d\n", base);
                if (sendto(sock, &ack, sizeof(ack), 0,
                     (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != sizeof(ack))
                    DieWithError("sendto() sent a different number of bytes than expected");
            } else if (base == -1) {
                printf("Sending Terminal ACK\n", base);
                if (sendto(sock, &ack, sizeof(ack), 0,
                     (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != sizeof(ack))
                    DieWithError("sendto() sent a different number of bytes than expected");
            }
        } else {
            printf("SIMULATED LOSE\n");
        }

    }
    /* NOT REACHED */
}

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

struct ACKPacket createACKPacket (int ack_type, int base){
        struct ACKPacket ack;
        ack.type = ack_type;
        ack.ack_no = base;
        return ack;
}
int is_lost(float loss_rate) {
    double rv;
    rv = drand48();
    if (rv < loss_rate)
    {
        return(1);
    } else {
        return(0);
    }
}