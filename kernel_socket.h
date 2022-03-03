#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

#include "tinyos.h"
#include "kernel_streams.h"

typedef enum socket_type{  //IP and PORT
  UNBOUND,
  PEER,
  LISTENER
}socket_type;

typedef struct listener_socket{
  rlnode request_queue; //collect requests
  CondVar list_cond;
}listener;

typedef struct peer_socket{
  PPCB* read_pipe;
  PPCB* write_pipe;
  socket_type peer_s;
}peer;

typedef struct socket_control_block{
  socket_type type;
  listener* listener;
  peer* peer;
  port_t port;

  unsigned int counter;
  Fid_t fid;
  FCB* fcb;
}SCB;

SCB* port_map[MAX_PORT+1];

typedef struct request{
  SCB* scb;
  int admit;
  rlnode reqNode;
  CondVar reqCondVar;
}request;

int socket_read(void* socket, char* buf, unsigned int size);
int socket_write(void* socket, const char* buf, unsigned int size);
int socket_close(void* fid);
int listener_close(void* socket);
int peer_close(void* socket);

#endif /* __KERNEL_SOCKET_H */
