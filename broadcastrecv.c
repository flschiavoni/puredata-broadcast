/* Plugin structure generated by Schiavoni Pure Data external Generator */
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "m_pd.h"

#define MAX_SIZE 1024 * 1024 // Max size for messages

static t_class *broadcastrecv_class;

// PD Type
typedef struct _broadcastrecv {
   t_object x_obj;
   t_int sockfd;
   t_int port;

   pthread_t receiver_thread;

   t_outlet *message_outlet;
} t_broadcastrecv;


// -------------------------------------------------------------------------
// Socket factory
// -------------------------------------------------------------------------
int broadcastrecv_create_socket(t_broadcastrecv *x){
   struct addrinfo hints, *servinfo;
   int rv;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags = AI_PASSIVE; // use my IP

   char temp_port[6];
   sprintf(temp_port,"%d", (int)x->port);
   if ((rv = getaddrinfo(NULL, temp_port, &hints, &servinfo)) != 0) {
      post("getaddrinfo: %s\n", gai_strerror(rv));
   }
   if ((x->sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,servinfo->ai_protocol)) == -1) {
      return 0;
   }
   int on = 1;
   if (setsockopt(x->sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on)) < 0){
      post("Could not set SO_REUSEADDR");
      return 0;
   }

   if (bind(x->sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
      close(x->sockfd);
      return 0;
   }
   freeaddrinfo(servinfo);
   return 1;
}

// -------------------------------------------------------------------------
// Message received
// -------------------------------------------------------------------------
void * broadcastrecv_received_message(void * arg){
   t_broadcastrecv * x = (t_broadcastrecv *) arg;
   while(sys_trylock());
   char * message = malloc(MAX_SIZE);
   int message_size;
   t_binbuf *b;
   t_atom *at; // create the atom array
   t_symbol *message_type; // Create the symbol to message selector
   int natom;
   b = binbuf_new(); // create a binbuff
   sys_unlock();

   while(1){
      message_size = recvfrom(x->sockfd, message, MAX_SIZE , 0, NULL, NULL); // Receive the broadcast message
      while(sys_trylock());
      binbuf_text(b, message, message_size);//Put the received message in binbuff
      natom = binbuf_getnatom(b); // Verify the argc in message
      at = binbuf_getvec(b); // transform the message in an atom list
      message_type = atom_getsymbolarg(0, natom, at); //Take the first atom as the message type
      outlet_anything(x->message_outlet, message_type, natom - 1, at+1);
      sys_unlock();
   }
}

// Constructor of the class
void * broadcastrecv_new(t_floatarg port) {
   t_broadcastrecv *x = (t_broadcastrecv *) pd_new(broadcastrecv_class);
   if(port != 0)
      x->port = port;
   else
      x->port = 40000;
   x->message_outlet = outlet_new(&x->x_obj, &s_anything);
   // Creates the socket
   if (broadcastrecv_create_socket(x) < 1){
      post("Problem creating receiver.\nDo you already have some network software binded to port %d?", x->port);
      return NULL;
   }
   // Creates the receive function
   pthread_create(&x->receiver_thread, NULL, broadcastrecv_received_message, x);
   return (void *) x;
}

// Destroy the class
void broadcastrecv_destroy(t_broadcastrecv *x) {
   pthread_cancel(x->receiver_thread);
   close(x->sockfd); // close the socket
}

void broadcastrecv_setup(void) {
   broadcastrecv_class = class_new(gensym("broadcastrecv"),
      (t_newmethod) broadcastrecv_new, // Constructor
      (t_method) broadcastrecv_destroy, // Destructor
      sizeof (t_broadcastrecv),
      CLASS_NOINLET,
      A_DEFFLOAT,
      0);//Must always ends with a zero
}
