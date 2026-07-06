/*Copyright 2026  Universidade Federal do Mato Grosso do Sul
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*    http://www.apache.org/licenses/LICENSE-2.0 
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef SIGBPF_H
#define SIGBPF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>  
#include <sys/types.h> 
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <assert.h>
#include <stddef.h>
#include <sys/signalfd.h>
#include <stdalign.h>

#include "include/spright.h"
#include "include/http.h"
#include "../ebpf/xsk_kern.skel.h"

#define INVALID_POSITION UINT64_MAX
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif


#define SIGBPF_NAME     "SIGBPF_MEM"
#define SIGBPF_MEMPOOL  "SIGBPF_MEMPOOL"
#define SIGBPF_TAM (1U << 16) * sizeof(struct http_transaction)
//#define SIGBPF_TAM (1U << 20) * sizeof(struct http_transaction)

#define N_ELEMENTS (1U << 16)
#define N_MASK (N_ELEMENTS - 1)

#define RINGBUF_REGION "RINGBUF_MEM"
#define RINGBUF_TAM sizeof(struct sigbpf_ringbuffer)


#define HASH_SIZE 1024 

extern struct xsk_kern *skel;
extern struct spright_cfg_s *sigbpf_cfg;
extern void *sigbpf_ptr;

extern int fd_sigbpf_mem,
           fd_sigbpf_mempool,
           fd_cfg_file;

//extern int fd_sigbpf_mempool;
//extern int fd_cfg_file;

struct sigbpf_ringbuffer{
	alignas(64)	uint64_t head;
	alignas(64)	uint64_t tail;
	alignas(64)	uint64_t ringbuffer[N_ELEMENTS];

}__attribute__((aligned(64)));

typedef struct {
    pid_t pid;
    int index;
} HashTable;
extern HashTable hash_t[HASH_SIZE];

extern struct sigbpf_ringbuffer *ringbuff;
extern uint64_t rb[N_ELEMENTS];

extern char temp[400];
extern char dir_temp[256];
extern pid_t pid_alvo;
extern int mapa_sinal_fd, map_fd;

//extern char *path_fixo;

extern void *sigbpf_mempool;

void *sigbpf_create_mem();
void *sigbpf_ptr_mem();

struct spright_cfg_s *sigbpf_cfg_mem();
struct spright_cfg_s *sigbpf_cfg_ptr();

int sigbpf_update_map(char *map_name, int fn_id, int pid, int *map_fd);
int sigbpf_lookup_map(char *map_name, int key);

void seek_elem_map(int fd_map_signal, int nf_id, int pid, int matriz[][2], HashTable *hash_t);
void lookup_map(int *fd_map_signal, int matriz[][2]);
void lookup_map_nf(int *fd_map_signal, int matriz[][2]);

struct sigbpf_ringbuffer *sigbpf_mempool_create();
struct sigbpf_ringbuffer *sigbpf_mempool_ptr();

//uint64_t sigbpf_mempool_get();
uint64_t allocate_object();
//int sigbpf_mempool_put( uint64_t addr);
int release_object( uint64_t addr);
struct http_transaction *sigbpf_mempool_access(void **temp, uint64_t addr);

#endif 
 

