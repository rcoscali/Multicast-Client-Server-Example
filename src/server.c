/* server.c
 * This sample demonstrates a multicast server that works with either
 * IPv4 or IPv6, depending on the multicast address given.
 *
 * Troubleshoot Windows: Make sure you have the IPv6 stack installed by running
 *     >ipv6 install
 *
 * Usage:
 *     server <Multicast Address> <Port> <packetsize> <defer_ms> [<TTL>>]
 *
 * Examples:
 *     >server 224.0.22.1 9210 6000 1000
 *     >server ff15::1 2001 65000 1
 *
 * Written by tmouse, July 2005
 * http://cboard.cprogramming.com/showthread.php?t=67469
 *
 * Modified to run multi-platform by Christian Beier <dontmind@freeshell.org>.
 */




#include <stdio.h>
#include <stdlib.h>
#ifdef UNIX
#include <unistd.h> /* for usleep() */
#endif
#include "msock.h"



static void
DieWithError(char* errorMessage)
{
  fprintf(stderr, "%s\n", errorMessage);
  exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
  SOCKET sock;
  struct addrinfo *multicastAddr;
  int 	    server_id;		    /* Id of server */
  char*     multicastIP;            /* Arg: IP Multicast address */
  char*     multicastPort;          /* Arg: Server port */
  char*     sendString;             /* Arg: String to multicast */
  int       sendStringLen;          /* Length of string to multicast */
  int       multicastTTL;           /* Arg: TTL of multicast packets */
  int       defer_ms;               /* miliseconds to defer in between sending */

  int i;
  
  if ( argc < 6 || argc > 7 )
    {
      fprintf(stderr, "Usage: %s <server_id> <Multicast Address> <Port> <packetsize> <defer_ms> [<TTL>]\n", argv[0]);
      exit(EXIT_FAILURE);
    }

  server_id     = atoi(argv[1]);
  if (server_id < 0 || server_id > 3)
    DieWithError("server ID have to be 0, 1, 2 or 3 !");
  multicastIP   = argv[2];             /* First arg:   multicast IP address */
  multicastPort = argv[3];             /* Second arg:  multicast port */
  sendStringLen = atoi(argv[4]);   
  defer_ms = atoi(argv[5]);
   
  /* just fill this with some byte */
  sendString = (char*)calloc(sendStringLen, sizeof(char));
  for (i = 0; i < sendStringLen; ++i)
    sendString[i] = 's';
     	
  multicastTTL = (argc == 7 ?         /* Fourth arg:  If supplied, use command-line */
		  atoi(argv[6]) : 1); /* specified TTL, else use default TTL of 1 */

  sock = mcast_send_socket(multicastIP, multicastPort, multicastTTL, &multicastAddr);
  if (sock == -1)
    DieWithError("mcast_send_socket() failed");
  
  int nr = server_id << 30;
  for (;;) /* Run forever */
    {
      unsigned int* p_nr = (int*)sendString;
      *p_nr = htonl(nr);
 
      if (sendto(sock, sendString, sendStringLen, 0,
		 multicastAddr->ai_addr, multicastAddr->ai_addrlen) != sendStringLen)
	DieWithError("sendto() sent a different number of bytes than expected");
        
      fprintf(stderr, "packet %u/%u sent\n", (unsigned)nr >> 30, nr & 0x3FFFFFFF);
      nr++;
#ifdef UNIX
      usleep(defer_ms*1000); 
#elif WIN32 
      Sleep (defer_ms);
#endif 
    }

  /* NOT REACHED */
  return 0;
}

