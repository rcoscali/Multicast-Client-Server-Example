/*
  msock.c - multicast socket creation routines

  (C) 2016 Christian Beier <dontmind@sdf.org>

*/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifdef UNIX
#include <unistd.h>
#endif

#include "msock.h"

static void
DieWithError(char* errorMessage)
{
  fprintf(stderr, "%s\n", errorMessage);
  exit(EXIT_FAILURE);
}

/*
 * mcast_send_socket
 *
 * This function creates a socket for sending datagrams from a multicast addrs on a given port.
 * The address info struct is returned.
 *
 * @params:
 *   @in 	ipaddr: 	multicast IPv4/IPv6 address in dotted notation
 *   @in 	port:		port used to send the datagram
 *   @in 	ttl:	Time to live of the packet to send
 *   @inout 	addrinfo:	Address info structure retreived from the dotted notation
 *
 * @return
 *   the socket file descriptor. -1 is something goes wrong.
 */
SOCKET
mcast_send_socket(char* ipaddr, char* port,  int ttl, struct addrinfo **addrinfo)
{

  SOCKET sock;
  struct addrinfo hints = { 0 };    /* Hints for name lookup */
    
    
#ifdef WIN32
  WSADATA trash;
  if(WSAStartup(MAKEWORD(2,0),&trash)!=0)
    DieWithError("Couldn't init Windows Sockets\n");
#endif

    
  /*
    Resolve destination address for multicast datagrams 
  */
  hints.ai_family   = PF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags    = AI_NUMERICHOST;
  int status;
  if ((status = getaddrinfo(ipaddr, port, &hints, addrinfo)) != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
      return -1;
    }

   

  /* 
     Create socket for sending multicast datagrams 
  */
  if ((sock = socket((*addrinfo)->ai_family, (*addrinfo)->ai_socktype, 0)) < 0)
    {
      perror("socket() failed");
      freeaddrinfo(*addrinfo);
      return -1;
    }

  /* 
     Set TTL of multicast packet 
  */
  if (setsockopt(sock,
		 (*addrinfo)->ai_family == PF_INET6 ? IPPROTO_IPV6        : IPPROTO_IP,
		 (*addrinfo)->ai_family == PF_INET6 ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL,
		 (char*) &ttl, sizeof(ttl)) != 0 )
    {
      perror("setsockopt() failed");
      freeaddrinfo(*addrinfo);
      return -1;
    }
    
    
  /* 
     set the sending interface 
  */
  if ((*addrinfo)->ai_family == PF_INET)
    {
      in_addr_t iface = INADDR_ANY; /* well, yeah, any */
      if(setsockopt(sock, 
		    IPPROTO_IP,
		    IP_MULTICAST_IF,
		    (char*)&iface, sizeof(iface)) != 0)
	{ 
	  perror("interface setsockopt() sending interface");
	  freeaddrinfo(*addrinfo);
	  return -1;
	}
    }
  else if ((*addrinfo)->ai_family == PF_INET6)
    {
      unsigned int ifindex = 0; /* 0 means 'default interface'*/
      if(setsockopt(sock, 
		    IPPROTO_IPV6,
		    IPV6_MULTICAST_IF,
		    (char*)&ifindex, sizeof(ifindex)) != 0)
	{ 
	  perror("interface setsockopt() sending interface");
	  freeaddrinfo(*addrinfo);
	  return -1;
	}
    }
  else
    {
      fprintf(stderr, "setsockopt: unknown address family\n");
      return -1;			
    }

     
  return sock;
}



/*
 * mcast_recv_socket
 *
 * This function creates a socket for receiving datagrams from a multicast addrs on a given port.
 * The receive buffer size to use is also provided.
 *
 * @params:
 *   @in 	ipaddr: 	multicast IPv4/IPv6 address in dotted notation
 *   @in 	port:		port used to send the datagram
 *   @in 	recv_buf_sz:	size of the receive buffer for multicast datagram
 *
 * @return
 *   the socket file descriptor. -1 is something goes wrong.
 */
SOCKET
mcast_recv_socket(char* ipaddr, char* port, int recv_buf_sz)
{

  SOCKET sock;
  struct addrinfo   hints  = { 0 };    		/* Hints for name lookup */
  struct addrinfo*  localAddr = 0, *la;        	/* Local address to bind to */
  struct addrinfo*  addrInfo = 0;     		/* Multicast Address */
  int yes=1;
  
#ifdef WIN32
  WSADATA trash;
  if(WSAStartup(MAKEWORD(2, 0), &trash) != 0)
    DieWithError("Couldn't init Windows Sockets\n");
#endif

    
  /* Resolve the multicast group address */
  hints.ai_family = PF_UNSPEC;
  hints.ai_flags  = AI_NUMERICHOST;
  int status;
  if ((status = getaddrinfo(ipaddr, NULL, &hints, &addrInfo)) != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
      goto error;
    }
  
  /* 
     Get a local address with the same family (IPv4 or IPv6) as our multicast group
     This is for receiving on a certain port.
  */
  hints.ai_family   = addrInfo->ai_family;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags    = AI_PASSIVE; /* Return an address we can bind to */
  if ((status = getaddrinfo(NULL, port, &hints, &localAddr)) != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
      goto error;
    }
  

  for (la = localAddr; la != NULL; la = la->ai_next)
    {
      /* Create socket for receiving datagrams */
      if ((sock = socket(localAddr->ai_family, localAddr->ai_socktype, 0)) == -1)
	continue;
    
      /*
       * Enable SO_REUSEADDR to allow multiple instances of this
       * application to receive copies of the multicast datagrams.
       */
      if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == 0)
	{
	  /* Bind the local address to the multicast port */
	  if (bind(sock, localAddr->ai_addr, localAddr->ai_addrlen) == 0)
	    {
	      /* success */
	      break; 
	    }
	}
      
      close(sock);
    }

  if (la == NULL)
    DieWithError("Couldn't bind the socket on one of the proposed address!\n");

  /* get/set socket receive buffer */
  int optval = 0;
  socklen_t optval_len = sizeof(optval);
  int dfltrcvbuf;
  if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optval_len) != 0)
    {
      perror("getsockopt");
      goto error;
    }
  dfltrcvbuf = optval;
  optval = recv_buf_sz;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, sizeof(optval)) != 0)
    {
      perror("setsockopt");
      goto error;
    }
  if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optval_len) != 0)
    {
      perror("getsockopt");
      goto error;
    }
  printf("tried to set socket receive buffer from %d to %d, got %d\n",
	 dfltrcvbuf, recv_buf_sz, optval);

  
    
    
  /* Join the multicast group. We do this seperately depending on whether we
   * are using IPv4 or IPv6. 
   */
  /* IPv4 */
  if (addrInfo->ai_family  == PF_INET &&  
      addrInfo->ai_addrlen == sizeof(struct sockaddr_in))
    {
      struct ip_mreq multicastRequest;  /* Multicast address join structure */

      /* Specify the multicast group */
      memcpy(&multicastRequest.imr_multiaddr,
	     &((struct sockaddr_in*)(addrInfo->ai_addr))->sin_addr,
	     sizeof(multicastRequest.imr_multiaddr));

      /* Accept multicast from any interface */
      multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);

      /* Join the multicast address */
      if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&multicastRequest, sizeof(multicastRequest)) != 0)
	{
	  perror("setsockopt() failed");
	  goto error;
	}
    }
  /* IPv6 */
  else if (addrInfo->ai_family  == PF_INET6 &&
	   addrInfo->ai_addrlen == sizeof(struct sockaddr_in6))
    {
      struct ipv6_mreq multicastRequest;  /* Multicast address join structure */

      /* Specify the multicast group */
      memcpy(&multicastRequest.ipv6mr_multiaddr,
	     &((struct sockaddr_in6*)(addrInfo->ai_addr))->sin6_addr,
	     sizeof(multicastRequest.ipv6mr_multiaddr));

      /* Accept multicast from any interface */
      multicastRequest.ipv6mr_interface = 0;

      /* Join the multicast address */
      if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*)&multicastRequest, sizeof(multicastRequest)) != 0)
	{
	  perror("setsockopt() failed");
	  goto error;
	}
    }
  else
    {
      perror("Neither IPv4 or IPv6"); 
      goto error;
    }


 doreturn:
  if(localAddr)
    freeaddrinfo(localAddr);
  if(addrInfo)
    freeaddrinfo(addrInfo);
    
  return sock;

 error:
  sock = -1;
  goto doreturn;
}
