/* client.c
 * This sample demonstrates a multicast client that works with either
 * IPv4 or IPv6, depending on the multicast address given.

 * Troubleshoot Windows: Make sure you have the IPv6 stack installed by running
 *     >ipv6 install
 *
 * Usage:
 *     client <Multicast IP> <Multicast Port> <Receive Buffer Size>
 *
 * Examples:
 *     >client 224.0.2.1 9210 70000
 *     >client ff15::1 2001 10000
 *
 * Written by tmouse, July 2005
 * http://cboard.cprogramming.com/showthread.php?t=67469
 * 
 * Modified to run multi-platform by Christian Beier <dontmind@freeshell.org>.
 */



#ifdef UNIX
# include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "msock.h" 

#define MAX_SERVER_IDS 4
#define MULTICAST_SO_RCVBUF 327680

SOCKET     sock;                     /* Socket */
char*      recvBuf;                  /* Buffer for received data */

/*
 * DieWithError
 *   Does exactly as it said, after having freed resources
 */
static void
DieWithError(char* errorMessage)
{
  fprintf(stderr, "%s\n", errorMessage);
  if (sock >= 0)
    close(sock);
  if (recvBuf)
    free(recvBuf);

  exit(EXIT_FAILURE);
}

/*
 * main
 *
 * @arguments:
 *
 * @return:
 *
 */
int
main(int argc, char* argv[])
{
  char*      multicastIP;              /* Arg: IP Multicast Address */
  char*      multicastPort;            /* Arg: Port */
  int        recvBufLen;               /* Length of receive buffer */

  if (argc == 1 ||
      (argc == 2 && (argv[1][0] == '-' && argv[1][1] == 'h' && argv[1][2] == 0)) ||
      argc != 4)
    {
      fprintf(stderr, "Usage: %s <Multicast IP> <Multicast Port> <Receive Buffer Size>\n\n", argv[0]);
      fprintf(stderr, "  ex.: %s 224.0.2.1 9210 1024\n", argv[0]);
      exit(EXIT_FAILURE);
    }

#ifdef WIN32
  WSADATA trash;
  if (WSAStartup(MAKEWORD(2, 0), &trash) != 0)
    DieWithError("Couldn't init Windows Sockets\n");
#endif

  multicastIP   = argv[1];      /* First arg:  Multicast IP address */
  multicastPort = argv[2];      /* Second arg: Multicast port */
  recvBufLen    = atoi(argv[3]);
 
  recvBuf = (char*)malloc(recvBufLen * sizeof(char));

  sock = mcast_recv_socket(multicastIP, multicastPort, MULTICAST_SO_RCVBUF);
  if(sock < 0)
    DieWithError("mcast_recv_socket() failed");

  int server_id;
  int rcvd = 0;
  int lost = 0;
    
  int last_p[MAX_SERVER_IDS];
  last_p[0] = 0;
  last_p[1] = 0;
  last_p[2] = 0;
  last_p[3] = 0;
  for (;;) /* Run forever */
    {
      time_t timer;
      int bytes = 0;

      /* Receive a single datagram from the server */
      if ((bytes = recvfrom(sock, recvBuf, recvBufLen, 0, NULL, 0)) < 0)
	DieWithError("recvfrom() failed");
        
      ++rcvd;
      unsigned int this_p = ntohl(*(int*)recvBuf);
      server_id = this_p >> 30;
      this_p = this_p & 0x3fffffff;

      if (last_p[server_id] >= 0) /* only check on the second and later runs */
	{
	  if (this_p - last_p[server_id] > 1)
	    lost += this_p - (last_p[server_id] + 1);
	}
      last_p[server_id] = this_p;
        
      /* Print the received string */
	  char datetime[128];
      time(&timer);  /* get time stamp to print with recieved data */
	  ctime_s(datetime, 128, &timer);
      printf("Packets recvd %d (%d,%d,%d,%d) lost %d, loss ratio %f    ", rcvd, last_p[0], last_p[1], last_p[2], last_p[3], lost, (double)lost/(double)(rcvd + lost));
      printf("Time Received: %.*s : packet (%d,%d) %d bytes\n", (int)strlen(datetime) - 1, datetime, server_id, this_p, bytes);
    }

  /* NOT REACHED */
  exit(EXIT_SUCCESS);
}

