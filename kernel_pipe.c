#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

file_ops reader_fops = {
		.Open=NULL,
		.Read=reader_read,
		.Write=NULL,
		.Close=reader_close
};

file_ops writer_fops = {
		.Open=NULL,
		.Read=NULL,
		.Write=writer_write,
		.Close=writer_close
};

int sys_Pipe(pipe_t* pipe)
{
	PPCB* ppcb = (PPCB*)xmalloc(sizeof(PPCB));

	FCB* fcb[2];
	Fid_t fid[2];
	fcb[0] = NULL;
	fcb[1] = NULL;

	int reserve=FCB_reserve(2, fid, fcb);

	if(reserve==1){

		pipe->read=fid[0];
		pipe->write=fid[1];

		ppcb->pip_t.read=fid[0];
		ppcb->pip_t.write=fid[1];

		ppcb->reader=fcb[0];
		ppcb->writer=fcb[1];

		ppcb->has_space=COND_INIT;
		ppcb->has_data=COND_INIT;

		ppcb->read_count=0;
		ppcb->write_count=0;
		ppcb->write_count2=0;
		ppcb->read_count2=0;

		//Stream functions initialization.
		fcb[0]->streamfunc = &reader_fops;
		fcb[1]->streamfunc = &writer_fops;
		ppcb->reader->streamfunc =&reader_fops;
		ppcb->writer->streamfunc =&writer_fops;

		//Initialize the stream objects of the FCBS.
		fcb[0]->streamobj = pipe;
		fcb[1]->streamobj = pipe;
		ppcb->reader->streamobj=ppcb;
		ppcb->writer->streamobj=ppcb;

		return 0;
	}

	return -1;
}

//Reader READ Function.
int reader_read(void* reader, char* buf, unsigned int s){

	PPCB* ppcb = (PPCB *)reader;

	int buffer_size = ppcb->write_count -ppcb->read_count;
	int take_size =0;
	int next=0;
	int size = s;

	//Failed get the object or Failed reader of pipe.
	if (ppcb == NULL || ppcb->reader == NULL){return -1;}

	//Reader has reached the writer but the writer is still alive!
	while (buffer_size == 0 && ppcb->pip_t.write != -1 )
		{
			kernel_broadcast(&ppcb->has_space);
			kernel_wait(&(ppcb->has_data), SCHED_PIPE);
	  	buffer_size = ppcb->write_count - ppcb->read_count;
	  }

	//EOF Reached.
	if (buffer_size == 0 && ppcb->pip_t.write == -1)
		{return 0;}

		while (size>0 &&  buffer_size >0) {

			if(size <= buffer_size){
					take_size = size;
				}else{
					take_size = buffer_size;
				}

		for (int i = 0; i < take_size; i++) {

			if(ppcb->read_count2 >= BUFFER_SIZE){

				ppcb->read_count2=0;
			}

			buf[i+next]=	ppcb->buf[ppcb->read_count2];

			ppcb->read_count++;
			ppcb->read_count2++;
		}

		size=size-take_size;

		next += take_size;
		buffer_size = ppcb->write_count - ppcb->read_count;
	}

		return s-size;
	}

//Writer WRITE Function.
int writer_write(void* writer, const char* buf, unsigned int s){

		PPCB* ppcb = (PPCB *)writer;

		int buffer_size = BUFFER_SIZE-( ppcb->write_count -ppcb->read_count);
		int take_size =0;
		int next=0;
		int size = s;

		//Failed get the object or Failed reader of pipe.
		if (ppcb == NULL || ppcb->writer == NULL || ppcb->reader == NULL){return -1;}

		//Writer has reached the reader but the reader is still alive!
		while (buffer_size == 0 && ppcb->pip_t.read != -1 )
		{
			kernel_broadcast(&ppcb->has_data);
			kernel_wait(&(ppcb->has_space), SCHED_PIPE);
		  buffer_size = BUFFER_SIZE - (ppcb->write_count-ppcb->read_count);
		 }

		//EOF Reached.
		if (buffer_size == 0 && ppcb->pip_t.read == -1){return 0;}

		while (size>0 && buffer_size >0) {

				if(size <= buffer_size){
						take_size = size;
				}else{
						take_size = buffer_size;
				}

				for (int i = 0; i < take_size; i++) {

					if(ppcb->write_count2 >= BUFFER_SIZE){

						ppcb->write_count2=0;

					}else{
						ppcb->buf[ppcb->write_count2]=buf[i+next];
					}

			ppcb->write_count++;
			ppcb->write_count2++;

		}
		size=size-take_size;
		next +=take_size;
		buffer_size= BUFFER_SIZE - (ppcb->write_count-ppcb->read_count);
	}
		return s- size;
	}


int reader_close(void* fid){

	PPCB* ppcb = (PPCB*) fid;

	if(ppcb==NULL) {return -1;}

	if(ppcb->reader->refcount==0){

		ppcb->pip_t.read = -1;
		ppcb->reader = NULL;

		if(ppcb->writer==NULL){
			free(ppcb);
			ppcb=NULL;
			return 0;
		}else{
			kernel_broadcast(&ppcb->has_space);
			return 0;
		}
	}
	return -1;
}

int writer_close(void* fid){

		PPCB* ppcb = (PPCB*) fid;

		if(ppcb==NULL) {return -1;}

		if(ppcb->writer->refcount==0){

			ppcb->pip_t.write = -1;
			ppcb->writer = NULL;

			if(ppcb->reader==NULL){
				free(ppcb);
				ppcb=NULL;
				return 0;
			}else{
				kernel_broadcast(&ppcb->has_data);
				return 0;
			}
		}
		return -1;
}
