
#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_socket.h"

file_ops socket_fops={
	.Open = NULL,
	.Read = NULL,
	.Write = NULL,
	.Close = socket_close
};

file_ops peer_fops ={
	.Open = NULL,
	.Read = socket_read,
	.Write = socket_write,
	.Close = peer_close
};

file_ops listener_fops ={
	.Open = NULL,
	.Read = NULL,
	.Write = NULL,
	.Close = listener_close
};

Fid_t sys_Socket(port_t port)
{
	if(port <0 || port >= MAX_PORT+1){return NOFILE;}

	SCB* scb = (SCB*)xmalloc(sizeof(SCB));

	FCB* fcb[1];
	Fid_t fid[1];

	fcb[0] = NULL;
	fid[0] = -1;

	int reserve=FCB_reserve(1, fid, fcb);

	if(reserve == 1){

		scb->fcb = fcb[0];
		scb->fid = fid[0];

		scb->fcb->streamobj=scb;
		scb->fcb->streamfunc=&socket_fops;

		scb->type = UNBOUND;
		scb->port = port;
		scb->counter =0;

		return fid[0];
	}

	free(scb);
	return NOFILE;
}

int sys_Listen(Fid_t sock)
{
	FCB* fcb = get_fcb(sock);
	if(fcb != NULL){
		SCB* scb = (SCB*)fcb->streamobj;

		if(scb != NULL && scb -> port != NOPORT ){

			if (port_map[scb->port] != NULL){return -1;}
			if (scb->type != UNBOUND){return -1;}

			scb->listener = (listener*)xmalloc(sizeof(listener));
			scb->peer = NULL;
			scb->type = LISTENER;
			scb->listener->list_cond = COND_INIT;
			fcb->streamfunc = &listener_fops;
			port_map[scb->port] = scb;

			rlnode_init(&scb->listener->request_queue, NULL);

			return 0;
		}
	}
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	FCB* fcb = get_fcb(lsock);

	if(fcb!=NULL && fcb->streamfunc != NULL){

		SCB* scb = fcb->streamobj;

		if(scb != NULL && (scb->port >NOPORT && scb->port <=MAX_PORT) && scb->type == LISTENER){

			if(is_rlist_empty(&scb->listener->request_queue)){

			kernel_wait(&scb->listener->list_cond, SCHED_PIPE);

			if(port_map[scb->port] == NULL){return NOFILE;}
			}

			Fid_t fcfid = Socket(NOPORT);

			if(fcfid == NOFILE){return NOFILE;}

			FCB* fcb = get_fcb(fcfid);
			SCB* server = fcb->streamobj;

			rlnode* rl_req_node = rlist_pop_front(&scb->listener->request_queue);
			request* req = rl_req_node->obj;
			SCB* client = req->scb;
			Fid_t scfid = client->fid;

			if (server == NULL || client == NULL) {return NOFILE;}

			PPCB* pipe1 = (PPCB*)xmalloc(sizeof(PPCB)); //write
			pipe1->reader = NULL;
			pipe1->writer = NULL;
			pipe1->has_space = COND_INIT;
			pipe1->has_data = COND_INIT;
			pipe1->read_count = 0;
			pipe1->write_count = 0;
			pipe1->read_count2 = 0;
			pipe1->write_count2 = 0;
			pipe1->pip_t.read = scfid;
			pipe1->pip_t.write = fcfid;

			PPCB* pipe2 = (PPCB*)xmalloc(sizeof(PPCB)); //read
			pipe2->reader = NULL;
			pipe2->writer = NULL;
			pipe2->has_space = COND_INIT;
			pipe2->has_data = COND_INIT;
			pipe2->read_count = 0;
			pipe2->write_count = 0;
			pipe2->read_count2 = 0;
			pipe2->write_count2 = 0;
			pipe2->pip_t.read = fcfid;
			pipe2->pip_t.write = scfid;

			if(pipe1 == NULL || pipe1 == NULL){return NOFILE;}

			server->type = PEER;
			server->peer->write_pipe = pipe1;
			server->peer->read_pipe = pipe2;
			server->fcb->streamfunc = &peer_fops;

			client->type = PEER;
			client->peer->write_pipe = pipe2;
			client->peer->read_pipe = pipe1;
			client->fcb->streamfunc = &peer_fops;

			req->admit = 1;
			kernel_signal(&req->reqCondVar);
			return fcfid;
		}
	}
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	FCB* fcb = get_fcb(sock);

	if(fcb != NULL &&  (port > NOPORT && port <= MAX_PORT)){

		SCB* client = fcb->streamobj;

		if(client == NULL || client->type != UNBOUND){return -1;}

		SCB* listener = port_map[port];

		if(listener != NULL && listener->type == LISTENER){

			request* req = (request*)xmalloc(sizeof(request));
			req->scb = client;
			rlnode_init(&req->reqNode, req);
			req->reqCondVar = COND_INIT;
			req->admit = 0;

			rlist_push_back(&listener->listener->request_queue, &req->reqNode);
			kernel_signal(&listener->listener->list_cond);

			client->counter++;
			if(!kernel_timedwait(&req->reqCondVar, SCHED_USER, timeout)) return -1;
			client->counter--;

			free(req);
			req = NULL;

			if(req->admit == 0){
				return 0;
			}else{
				return -1;
			}
		}
	}

	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	FCB* fcb = get_fcb(sock);

	if(fcb != NULL && how>=1 && how <=3){

		SCB* scb = fcb->streamobj;

		if(scb != NULL && scb->type == PEER){

				if(how == 1){ //SHUTDOWN_READ

					reader_close(&scb->peer->read_pipe);

				}else if(how == 1){ //SHUTDOWN_WRITE

					writer_close(&scb->peer->write_pipe);

				}else if(how == 3){ //SHUTDOWN_BOTH

					reader_close(&scb->peer->read_pipe);
					writer_close(&scb->peer->write_pipe);

				}
				return 0;
		}
	}
	return -1;
}

int socket_close(void* fid){
	SCB* scb = (SCB*)fid;

	if(scb != NULL){
		free(scb);
		scb = NULL;
		return 0;
	}

	return -1;
}

int listener_close(void* socket){

	SCB* scb = (SCB*)socket;
	port_map[scb->port] = NULL;
	kernel_signal(&scb->listener->list_cond);
	socket_close(scb);

	return -1;
}

int peer_close(void* socket){
	SCB* scb = (SCB*)socket;

	if(scb->counter == 0){

		reader_close(scb->peer->read_pipe);
		writer_close(scb->peer->write_pipe);
		socket_close(scb);
		return 0;
	}

	return -1;
}

int socket_read(void* socket, char* buf, unsigned int size){
	SCB* scb = (SCB*)socket;

	if(scb->type == PEER){
		return reader_read(&scb->peer->read_pipe, buf, size);
	}

	return -1;
}

int socket_write(void* socket, const char* buf, unsigned int size){
	SCB* scb = (SCB*)socket;

	if(scb->type == PEER){
		return writer_write(&scb->peer->write_pipe, buf, size);
	}

	return -1;
}
