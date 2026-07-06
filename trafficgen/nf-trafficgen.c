// trafficgen.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define NS_PER_SEC 1000000000ULL

#ifndef EPOLL_MAX_EVENTS
#define EPOLL_MAX_EVENTS 1024
#endif

static inline uint64_t nsec_now(void)
{
	struct timespec ts;
	
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline void sleep_ns(uint64_t ns)
{
	uint64_t end = nsec_now() + ns;
	
	while (nsec_now() < end) ;
}

static inline int set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	
	if (flags < 0)
		return -1;
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return -1;
	return 0;
}

typedef struct {
	uint64_t send_ns;
	int active;
    	int request_type;  // Added a field to store the request type
}  fd_entry_t;

typedef struct
{
	const char *host;
	const char *port;
	const char *path;

	long seed;		// Initial seed for the random number generator
	double avg_rps;         // average requests per second (Poisson rate)
	int total_requests;
	int max_inflight;

	int epfd;

	fd_entry_t *fdtab;
	int fdtab_len;
	pthread_mutex_t fdtab_mu;

	double   *lat_us;
    	int      *request_types;  // Added a new array to store the request types
	
	uint64_t *req_ps_recv;	
	uint64_t *req_ps_sent;
	
	atomic_int lat_idx;

	atomic_int inflight;
	atomic_bool sender_done;
	atomic_bool stop;
}  ctx_t;

static int resolve_and_connect_blocking(const char *host, const char *port)
{
	struct addrinfo hints;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;

	struct addrinfo *res = NULL;
	int rc = getaddrinfo(host, port, &hints, &res);
	if (rc != 0) {
		fprintf(stderr, "getaddrinfo(%s,%s): %s\n", host, port, gai_strerror(rc));
		return -1;
	}

	int fd = -1;
	for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0)
			continue;

		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
			break;
		}

		close(fd);
		fd = -1;
	}

	freeaddrinfo(res);
	return fd;
}

// Exponential interarrival: X ~ Exp(rate), where rate = avg_rps.
// Returns seconds.
static double exp_interarrival(double rate, struct drand48_data *rng)
{
	double u;
	do {
		drand48_r(rng, &u);
	} while (u <= 0.0);
	return -log(u) / rate;
}

static void fdtab_set(ctx_t *c, int fd, uint64_t send_ns)
{
	if (fd < 0 || fd >= c->fdtab_len)
		return;
	pthread_mutex_lock(&c->fdtab_mu);
	c->fdtab[fd].send_ns = send_ns;
	c->fdtab[fd].active = 1;
	pthread_mutex_unlock(&c->fdtab_mu);
}

static bool fdtab_get_and_clear(ctx_t *c, int fd, uint64_t *send_ns_out)
{
	if (fd < 0 || fd >= c->fdtab_len)
		return false;
	bool ok = false;
	pthread_mutex_lock(&c->fdtab_mu);
	if (c->fdtab[fd].active) {
		*send_ns_out = c->fdtab[fd].send_ns;
		c->fdtab[fd].active = 0;
		ok = true;
	}
	pthread_mutex_unlock(&c->fdtab_mu);
	return ok;
}

static int epoll_add_fd(int epfd, int fd)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
	ev.data.fd = fd;
	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static int epoll_del_fd(int epfd, int fd)
{
	return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

static void close_and_cleanup(ctx_t *c, int fd)
{
	epoll_del_fd(c->epfd, fd);
	close(fd);
	atomic_fetch_sub(&c->inflight, 1);
}

/*********************************************/

char *products[] = {"0PUK6V6EV0" , "1YMWWN1N4O", "2ZYFJ3GM2N", "66VCHSJNUP", "6E92ZMYYFZ", "9SIQT8TOJO", "L9ECAV7KIM", "LS4PSXUNUM", "OLJCESPC7Z"};
char *currency[] = {"EUR", "USD", "JPY", "CAD"};

char data_addCart[200];
char data_checkout[400];

// GET /1
//static inline void get_1(int req_num, char *buff){
static __always_inline void get_1( char *buff){

    sprintf(buff,"GET /1 HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "User-Agent: curl/8.5.0\r\n"
            "Accept: */*\r\n");

    return;
}

// POST /1/setCurrency 
//static inline void post_setCurrency(int req_num, char *buff){
static __always_inline void post_setCurrency( char *buff){

    int aux_rand = rand()%3;

    sprintf(buff, "POST /1/setCurrency?currency_code=%s HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %lu\r\n"
            "\r\n",
            currency[aux_rand] , strlen(buff)  );

    return;
}

// GET /1/cart
//static inline void get_cart(int req_num, char *buff){
static __always_inline void get_cart( char *buff){

    sprintf(buff,"GET /1/cart HTTP/1.1\r\n"
                    "Host: localhost:8080\r\n"
                    "User-Agent: curl/8.5.0\r\n"
                    "Accept: */*\r\n");

    return;
}

// GET /1/product
//static inline void get_product(int req_num, char *buff){
static __always_inline void get_product( char *buff){
    
    int aux_rand = rand()%9;

    sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
    sprintf(buff,"GET /1/product?%s HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "User-Agent: curl/8.5.0\r\n"
            "Connection: keep-alive\r\n"
            "Accept: */*\r\n\n"
            , products[aux_rand]);


    return;
}

// GET  /1/product
// POST /1/cart?product
//static inline void post_addtocart(int req_num, char *buff){
static __always_inline void post_addtocart( char *buff){

    int aux_rand = rand()%9;
    sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
    //sprintf(method, "POST;/1/cart/?%s", data_addCart);
    sprintf(buff, "POST /1/cart?%s HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %lu\r\n"
            "\r\n",
            data_addCart, sizeof(data_addCart) );
    return;
}

/*
 * CH-1: index
 * CH-2: index -> post_setcurrency
 * CH-3: index -> get_product
 * CH-4: index -> get_cart
 * CH-5: index -> get_product -> post_cart
 */

/*********************************************/

char req_1[2048], req_2[2048], req_3[2048], req_4[2048], req_5[2048], req_6[2048];


void set_req(const char *type_req){

	//char data_addCart[204];
	//int aux_rand = rand()%9;
	sprintf(req_1,"GET %s HTTP/1.1\r\n"
			"Host: localhost:8080\r\n"
			"User-Agent: curl/8.5.0\r\n"
			"Accept: */*\r\n", type_req);


	//sprintf(req_2, "POST /1/setCurrency?currency_code=%s HTTP/1.1\r\n"
	//		"Host: localhost:8080\r\n"
	//		"Content-Type: text/html\r\n"
	//		"Content-Length: %lu\r\n"
	//		"\r\n",
	//		currency[aux_rand] , strlen(req_2)  );

	//sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
	//sprintf(req_3,"GET /1/product?%s HTTP/1.1\r\n"
	//		"Host: localhost:8080\r\n"
	//		"User-Agent: curl/8.5.0\r\n"
	//		"Connection: keep-alive\r\n"
	//		"Accept: */*\r\n\n"
	//		, products[aux_rand]);

	//sprintf(req_4,"GET /1/cart HTTP/1.1\r\n"
	//		"Host: localhost:8080\r\n"
	//		"User-Agent: curl/8.5.0\r\n"
	//		"Accept: */*\r\n");

	//sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
	//sprintf(req_5, "POST /1/cart?%s HTTP/1.1\r\n"
	//		"Host: localhost:8080\r\n"
	//		"Content-Type: text/html\r\n"
	//		"Content-Length: %lu\r\n"
	//		"\r\n",
	//		data_addCart, sizeof(data_addCart) );

	//char *aux="/1/cart/checkout?email=someone@example.com&street_address=1600-Amphitheatre-Parkway&zip_code=94043&city=Mountain-View&state=CA&country=United-States&credit_card_number=4432801561520454&credit_card_expiration_month=1&credit_card_expiration_year=2039&credit_card_cvv=672";
	//sprintf(req_6, "POST /1/cart/checkout?%s HTTP/1.1\r\n"
	//		"Host: localhost:8080\r\n"
	//		"Content-Type: text/html\r\n"
	//		"Content-Length: %lu\r\n"
	//		"\r\n",
	//		aux, sizeof(aux) );

}


int sortear_proxima_tarefa() {
    int r = rand() % 18;
    //int r = rand() % 19;
    if (r < 1)  return 0; // index (Peso 1)
    if (r < 3)  return 1; // setCurrency (Peso 2)
    if (r < 13) return 2; // browseProduct (Peso 10)
    if (r < 15) return 3; // addToCart (Peso 2)
    //if (r < 18) return 4; // viewCart (Peso 3)
    return 4;             // viewCart (Peso 3)
    //return 5;             // checkout(Peso 1)
}

static void *sender_thread(void *arg)
{
	ctx_t *c = (ctx_t *)arg;

	struct drand48_data rng;
	srand48_r(c->seed, &rng);

	//srand(time(NULL));
	//char req[2048];
	char *req;
    	//int ch=0, step=0, flag = 1;
	
	set_req(c->path);
	req = req_1;
	
	//snprintf(req, sizeof(req),
	//	 "GET %s HTTP/1.1\r\n"
	//	 "Host: %s\r\n"
	//	 "User-Agent: http_poisson_epoll/1.0\r\n"
	//	 "Accept: */*\r\n"
	//	 "Connection: close\r\n"
	//	 "\r\n",
	//	 c->path, c->host);
	
	size_t req_len = strlen(req);

	uint64_t *sleep_for_ns = calloc((size_t)c->total_requests, sizeof(uint64_t));
	uint64_t prev = 0;
	for (int i = 0; i < c->total_requests; i++) {
		// Interarrival based on avg_rps (rate)
		double dt = exp_interarrival(c->avg_rps, &rng);		
		sleep_for_ns[i] = prev + (uint64_t)(dt * 1e9);
		prev = sleep_for_ns[i];
	}
			
	uint64_t t_start = nsec_now();
	for (int i = 0; i < c->total_requests && !atomic_load(&c->stop); i++) {
		int sleep_delta = (t_start + sleep_for_ns[i]) - nsec_now();
		if (sleep_delta > 0)
			sleep_ns(sleep_delta);

		// Backpressure on max inflight
		while (!atomic_load(&c->stop)) {
			int in = atomic_load(&c->inflight);
			if (in < c->max_inflight)
				break;
			printf("Backpressure activated\n");
			sleep_ns(1000000ull); // 1ms
		}
		if (atomic_load(&c->stop))
			break;

		int fd = resolve_and_connect_blocking(c->host, c->port);
		if (fd < 0) {
			fprintf(stderr, "connect failed (request %d)\n", i);
			continue;
		}

		if (set_nonblocking(fd) != 0) {
			perror("set_nonblocking");
			close(fd);
			continue;
		}

		atomic_fetch_add(&c->inflight, 1);

		req_len = strlen(req);

		/**************************************************************/

		ssize_t n = send(fd, req, req_len, 0);
		if (n < 0) {
			perror("send");
			close(fd);
			atomic_fetch_sub(&c->inflight, 1);
			continue;
		}

		uint64_t t_send = nsec_now();
		fdtab_set(c, fd, t_send);

		if (epoll_add_fd(c->epfd, fd) != 0) {
			perror("epoll_ctl(ADD)");
			uint64_t dummy;
			fdtab_get_and_clear(c, fd, &dummy);
			close(fd);
			atomic_fetch_sub(&c->inflight, 1);
			continue;
		}

	}

	atomic_store(&c->sender_done, true);
	return NULL;
}

//static void *sender_thread(void *arg)
//{
//	ctx_t *c = (ctx_t *)arg;
//
//	struct drand48_data rng;
//	srand48_r(c->seed, &rng);
//
//	//srand(time(NULL));
//	//char req[2048];
//	char *req;
//    	int ch=0, step=0, flag = 1;
//	
//	//get_1(req);
//	set_req();
//	req = req_1;
//	
//	//snprintf(req, sizeof(req),
//	//	 "GET %s HTTP/1.1\r\n"
//	//	 "Host: %s\r\n"
//	//	 "User-Agent: http_poisson_epoll/1.0\r\n"
//	//	 "Accept: */*\r\n"
//	//	 "Connection: close\r\n"
//	//	 "\r\n",
//	//	 c->path, c->host);
//	
//	int aux_rand = rand()%9;
//	size_t req_len = strlen(req);
//
//	uint64_t *sleep_for_ns = calloc((size_t)c->total_requests, sizeof(uint64_t));
//	uint64_t prev = 0;
//	for (int i = 0; i < c->total_requests; i++) {
//		// Interarrival based on avg_rps (rate)
//		double dt = exp_interarrival(c->avg_rps, &rng);		
//		sleep_for_ns[i] = prev + (uint64_t)(dt * 1e9);
//		prev = sleep_for_ns[i];
//	}
//			
//	uint64_t t_start = nsec_now();
//	for (int i = 0; i < c->total_requests && !atomic_load(&c->stop); i++) {
//		int sleep_delta = (t_start + sleep_for_ns[i]) - nsec_now();
//		if (sleep_delta > 0)
//			sleep_ns(sleep_delta);
//
//		// Backpressure on max inflight
//		while (!atomic_load(&c->stop)) {
//			int in = atomic_load(&c->inflight);
//			if (in < c->max_inflight)
//				break;
//			printf("Backpressure activated\n");
//			sleep_ns(1000000ull); // 1ms
//		}
//		if (atomic_load(&c->stop))
//			break;
//
//		int fd = resolve_and_connect_blocking(c->host, c->port);
//		if (fd < 0) {
//			fprintf(stderr, "connect failed (request %d)\n", i);
//			continue;
//		}
//
//		if (set_nonblocking(fd) != 0) {
//			perror("set_nonblocking");
//			close(fd);
//			continue;
//		}
//
//		atomic_fetch_add(&c->inflight, 1);
//
//		// SEND - TROCAR PARA SWITCH DPS
//		switch (ch){  // GET /1
//			case( 0):
//
//				//get_1(req);
//				step = 0;
//				//sprintf(req,"GET /1 HTTP/1.1\r\n"
//				//		"Host: localhost:8080\r\n"
//				//		"User-Agent: curl/8.5.0\r\n"
//				//		"Accept: */*\r\n");
//
//				req = req_1;
//                c->fdtab[fd].request_type = 0;  // Set the request type
//
//				ch = rand() % 5;
//
//				break;
//
//		/**************************************************************/
//			case( 1):   // POST /1/currency
//
//				if(step == 0){  // primeira chamada de CH-1 GET /1
//					//get_1(req);
//					step++;
//					//flag = 0;
//					//sprintf(req,"GET /1 HTTP/1.1\r\n"
//					//		"Host: localhost:8080\r\n"
//					//		"User-Agent: curl/8.5.0\r\n"
//					//		"Accept: */*\r\n");
//
//
//					req = req_1;
//                    c->fdtab[fd].request_type = 0;  // Set the request type
//					break;
//				}
//				//else(step == 1){  // enviando o POST
//				else{  // enviando o POST
//					//post_setCurrency(req);
//					aux_rand = rand()%3;
//
//					//sprintf(req, "POST /1/setCurrency?currency_code=%s HTTP/1.1\r\n"
//					//		"Host: localhost:8080\r\n"
//					//		"Content-Type: text/html\r\n"
//					//		"Content-Length: %lu\r\n"
//					//		"\r\n",
//					//		currency[aux_rand] , strlen(req)  );
//
//
//					req = req_2;
//                    c->fdtab[fd].request_type = 1;  // Set the request type
//					step = 0; // limpa step para o proximo CH-X
//					//flag = 1;
//
//
//					ch = rand() % 5;
//					break;	
//				}
//
//		/**************************************************************/
//			case( 2):
//
//				if(step == 0){  // primeira chamada de CH-1 GET /1
//					//get_1(req);
//					step++;
//					//flag = 0;
//					//sprintf(req,"GET /1 HTTP/1.1\r\n"
//					//		"Host: localhost:8080\r\n"
//					//		"User-Agent: curl/8.5.0\r\n"
//					//		"Accept: */*\r\n");
//
//					req = req_1;
//                    c->fdtab[fd].request_type = 0;  // Set the request type
//
//					break;
//				}
//				//else(step == 1){ 
//				else{ 
//					//get_product(req);
//					step = 0; // limpa step para o proximo CH-X
//					//flag = 1;
//
//					
//
//				//	sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
//				//	sprintf(req,"GET /1/product?%s HTTP/1.1\r\n"
//				//			"Host: localhost:8080\r\n"
//				//			"User-Agent: curl/8.5.0\r\n"
//				//			"Connection: keep-alive\r\n"
//				//			"Accept: */*\r\n\n"
//				//			, products[aux_rand]);
//
//
//					req = req_3;
//                    c->fdtab[fd].request_type = 2;  // Set the request type
//
//					ch = rand() % 5;
//					break;
//
//				}
//
//		/**************************************************************/
//			case( 3):
//
//				if(step == 0){  // primeira chamada de CH-1 GET /1
//					//get_1(req);
//					step++;
//				//	sprintf(req,"GET /1 HTTP/1.1\r\n"
//				//			"Host: localhost:8080\r\n"
//				//			"User-Agent: curl/8.5.0\r\n"
//				//			"Accept: */*\r\n");
//
//
//					req = req_1;
//                    c->fdtab[fd].request_type = 1;  // Set the request type
//					break;
//				}
//				else{
//					//get_cart(req);
//					step = 0; // limpa step para o proximo CH-X
//
//				//	sprintf(req,"GET /1/cart HTTP/1.1\r\n"
//				//			"Host: localhost:8080\r\n"
//				//			"User-Agent: curl/8.5.0\r\n"
//				//			"Accept: */*\r\n");
//
//
//					req = req_4;
//                    c->fdtab[fd].request_type = 3;  // Set the request type
//
//					ch = rand() % 5;
//					break;
//				}
//
//		/**************************************************************/
//			case( 4):
//
//				if(step == 0){  // primeira chamada de CH-1 GET /1
//					//get_1(req);
//					step++;
//				//	sprintf(req,"GET /1 HTTP/1.1\r\n"
//				//			"Host: localhost:8080\r\n"
//				//			"User-Agent: curl/8.5.0\r\n"
//				//			"Accept: */*\r\n");
//
//					req = req_1;
//                    			c->fdtab[fd].request_type = 1;  // Set the request type
//
//					break;
//				}
//				else if(step == 1){
//					//get_product(req);
//					aux_rand = rand()%9;
//
//				//	sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
//				//	sprintf(req,"GET /1/product?%s HTTP/1.1\r\n"
//				//			"Host: localhost:8080\r\n"
//				//			"User-Agent: curl/8.5.0\r\n"
//				//			"Connection: keep-alive\r\n"
//				//			"Accept: */*\r\n\n"
//				//			, products[aux_rand]);
//
//					req = req_3;
//                    c->fdtab[fd].request_type = 3;  // Set the request type
//
//					step++; 
//					break;
//				}
//				else{
//					post_addtocart(req);
//					step = 0;
//					//aux_rand = rand()%9;
//
//					//sprintf(data_addCart,"product_id=%s&quantity=%d", products[aux_rand], aux_rand);
//					//sprintf(req, "POST /1/cart?%s HTTP/1.1\r\n"
//					//		"Host: localhost:8080\r\n"
//					//		"Content-Type: text/html\r\n"
//					//		"Content-Length: %lu\r\n"
//					//		"\r\n",
//					//		data_addCart, sizeof(data_addCart) );
//
//
//					req = req_5;
//                    c->fdtab[fd].request_type = 4;  // Set the request type
//					ch = rand() % 5;
//					break;
//				}
//		}
//		req_len = strlen(req);
//
//		/**************************************************************/
//
//		ssize_t n = send(fd, req, req_len, 0);
//		if (n < 0) {
//			perror("send");
//			close(fd);
//			atomic_fetch_sub(&c->inflight, 1);
//			continue;
//		}
//
//		uint64_t t_send = nsec_now();
//		fdtab_set(c, fd, t_send);
//
//		if (epoll_add_fd(c->epfd, fd) != 0) {
//			perror("epoll_ctl(ADD)");
//			uint64_t dummy;
//			fdtab_get_and_clear(c, fd, &dummy);
//			close(fd);
//			atomic_fetch_sub(&c->inflight, 1);
//			continue;
//		}
//
//		// alteracao
//		// se flag == 1 nova CH-X a ser gerada
//		// se flag == 0 ainda esta enviando reqs da CH-X
//		//if (flag == 1){
//		//	ch = rand() % 5;
//		//}
//
//		//memset(req, 0, sizeof(req));
//		//make_request(rand()%5 , req );
//		//req_len = strlen(req);
//
//	}
//
//	atomic_store(&c->sender_done, true);
//	return NULL;
//}





static void *receiver_thread(void *arg)
{
	ctx_t *c = (ctx_t *)arg;

	struct epoll_event events[EPOLL_MAX_EVENTS];
	char buf[8192];

	while (!atomic_load(&c->stop)) {
		int timeout_ms = 200;
		int n = epoll_wait(c->epfd, events, EPOLL_MAX_EVENTS, timeout_ms);
		if (n < 0) {
			if (errno == EINTR) continue;
			perror("epoll_wait");
			break;
		}

		for (int i = 0; i < n; i++) {
			int fd = events[i].data.fd;

			uint64_t send_ns = 0;
			bool have_ts = fdtab_get_and_clear(c, fd, &send_ns);

			ssize_t r = recv(fd, buf, sizeof(buf), 0);
			if (r < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					if (have_ts)
						fdtab_set(c, fd, send_ns);
					continue;
				}
				close_and_cleanup(c, fd);
				continue;
			}

			uint64_t now_ns = nsec_now();
			if (have_ts) {
				double us = (double)(now_ns - send_ns) / 1e3;
				int idx = atomic_fetch_add(&c->lat_idx, 1);
				if (idx < c->total_requests) {
					c->lat_us[idx] = us;
					c->req_ps_recv[idx] = now_ns;
					c->req_ps_sent[idx] = send_ns;
					c->request_types[idx] = c->fdtab[fd].request_type; // Store the request type
				}
			}

			while (r > 0) {
				r = recv(fd, buf, sizeof(buf), 0);
				if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
					break;
			}
			close_and_cleanup(c, fd);
		}

		if (atomic_load(&c->sender_done) && atomic_load(&c->inflight) == 0) {
			break;
		}
	}

	return NULL;
}

static int cmp_double(const void *a, const void *b)
{
	double da = *(const double *)a;
	double db = *(const double *)b;
	return (da < db) ? -1 : (da > db) ? 1 : 0;
}

//static void print_stats(double *x, int *y, int n)
static void print_stats(double *x, int n)
{
	if (n <= 0) {
		printf("No samples collected.\n");
		return;
	}
	double sum = 0.0;
	double mn = x[0], mx = x[0];
	for (int i = 0; i < n; i++) {
		sum += x[i];
		if (x[i] < mn) mn = x[i];
		if (x[i] > mx) mx = x[i];
	}
	double mean = sum / n;

	double *copy = malloc((size_t)n * sizeof(double));
	if (!copy) {
		printf("min=%.3f us  mean=%.3f us  max=%.3f us  (n=%d)\n", mn, mean, mx, n);
		return;
	}
	memcpy(copy, x, (size_t)n * sizeof(double));
	qsort(copy, (size_t)n, sizeof(double), cmp_double);

	int i50 = (int)floor(0.50 * (n - 1));
	int i95 = (int)floor(0.95 * (n - 1));
	int i99 = (int)floor(0.99 * (n - 1));

	printf("Samples: %d\n", n);
	printf("min   %.3f us\n", copy[0]);
	printf("p50   %.3f us\n", copy[i50]);
	printf("p95   %.3f us\n", copy[i95]);
	printf("p99   %.3f us\n", copy[i99]);
	printf("mean  %.3f us\n", mean);
	printf("max   %.3f us\n", copy[n - 1]);

	free(copy);
}

int save_events_per_second(const char *filename,
                           const uint64_t *timestamps_ns,
                           size_t n)
{
    if (filename == NULL || timestamps_ns == NULL || n == 0) {
        return -1;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }

    // First timestamp defines time origin
    uint64_t start_sec = timestamps_ns[0] / NS_PER_SEC;
    uint64_t current_sec = start_sec;
    uint64_t count = 0;

    size_t i = 0;
    double sum = 0.;
    int first_bucket = 1;
    
    while (i < n) {
        uint64_t ts_sec = timestamps_ns[i] / NS_PER_SEC;

        if (ts_sec == current_sec) {
            count++;
            i++;
        } else {
            // Write current bucket
            fprintf(f, "%llu %llu\n",
                    (unsigned long long)(current_sec - start_sec),
                    (unsigned long long)count);

	    if (first_bucket)
		    first_bucket = 0;
	    else
		    sum += count;
            // Fill empty seconds if there is a gap
            while (++current_sec < ts_sec) {
                fprintf(f, "%llu 0\n",
                        (unsigned long long)(current_sec - start_sec));
            }

            // Move to next second
            current_sec = ts_sec;
            count = 0;
        }
    }

    // Write final bucket
    fprintf(f, "%llu %llu\n",
            (unsigned long long)(current_sec - start_sec),
            (unsigned long long)count);

    printf("\nAverage of %s: %f\n", filename, sum / (current_sec - start_sec - 1));
    fclose(f);
    return 0;
}

int save_latencies(const char *filename,
		   const double *timestamps_us,
		   size_t n,
	   	   int *request_types)
{
    if (filename == NULL || timestamps_us == NULL || n == 0) {
        return -1;
    }
    // Nomes para o log final
    const char *req_names[] = {
        "GET_INDEX",
        "POST_CURRENCY",
        "GET_PRODUCT",
        "GET_CART",
        "POST_CART",
        "POST_CHECKOUT"
    };


    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }

    size_t i = 0;

    while (i < n){
	    fprintf(f, "%s %f\n", req_names[ request_types[(i)] ], timestamps_us[i] );
	    i++;
    }

    fclose(f);
    return 0;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s <host> <port> <path> <avg_rps> <total_time> <max_inflight> <seed>\n"
		"\n"
		"Meaning:\n"
		"  host           = server IP address or name.\n"
		"  port           = TCP port of the server.\n"
		"  avg_rps        = average requests per second (Poisson rate).\n"
		"  total_time     = total time in seconds.\n"
		"  max_inflight   = max number of connections being processed.\n" 
		"  seed           = initial seed of the random number generator.\n" 
		"\n"
		"Example:\n"
		"  %s 10.10.1.1 8080 /1 7000 180 1000 56378234\n",
		prog, prog);
}

void set_cpu_affinity(pthread_t thread, int core_id){

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        fprintf(stderr, "Erro ao definir afinidade no Core %d\n", core_id);
    }
}


int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);

	if (argc != 8) {
		usage(argv[0]);
		return 1;
	}

	char settar_cpuf[256];
	sprintf(settar_cpuf, "taskset -cp 0 %d", getpid());
	int ret_sys = system(settar_cpuf);
	if( ret_sys == -1 || ret_sys == 127 ){
		perror("Error to pinn CPU");
		exit(1);
	}

	char reuse_ports[256];
	sprintf(reuse_ports, " sudo sysctl -w net.ipv4.tcp_tw_reuse=1");
	int ret_port = system(reuse_ports);
	if( ret_port  == -1 || ret_port == 127 ){
		perror("Error to pinn CPU");
		exit(1);
	}


	ctx_t c;
	memset(&c, 0, sizeof(c));
	c.host           = argv[1];
	c.port           = argv[2];
	c.path           = argv[3];
	c.avg_rps        = atof(argv[4]);
	c.total_requests = atoi(argv[5]) * c.avg_rps;
	c.max_inflight   = atoi(argv[6]);
	c.seed           = atol(argv[7]);
	if (c.avg_rps <= 0 || c.total_requests <= 0 || c.max_inflight <= 0 || c.seed <= 0) {
		usage(argv[0]);
		return 1;
	}
    


	c.epfd = epoll_create1(0);
	if (c.epfd < 0) {
		perror("epoll_create1");
		return 1;
	}

	long open_max = sysconf(_SC_OPEN_MAX);
	if (open_max < 0)
		open_max = 65536;
	if (open_max > 200000)
		open_max = 200000;

	c.fdtab_len = (int)open_max;
	c.fdtab = calloc((size_t)c.fdtab_len, sizeof(fd_entry_t));
	if (!c.fdtab) {
		perror("calloc(fdtab)");
		close(c.epfd);
		return 1;
	}
	pthread_mutex_init(&c.fdtab_mu, NULL);

	c.lat_us = calloc((size_t)c.total_requests, sizeof(double));
	if (!c.lat_us) {
		perror("calloc(lat_us)");
		free(c.fdtab);
		close(c.epfd);
		return 1;
	}

    c.request_types = calloc((size_t)c.total_requests, sizeof(int));
    if (!c.request_types) {
        perror("calloc(request_types)");
        free(c.fdtab);
        close(c.epfd);
        free(c.lat_us);
        free(c.req_ps_recv);
        free(c.req_ps_sent);
        return 1;
    }

	c.req_ps_recv = calloc((size_t)c.total_requests, sizeof(uint64_t));
	if (!c.req_ps_recv) {
		perror("calloc(req_ps_recv)");
		free(c.fdtab);
		close(c.epfd);
		free(c.lat_us);
		return 1;
	}
	

	c.req_ps_sent = calloc((size_t)c.total_requests, sizeof(uint64_t));
	if (!c.req_ps_sent) {
		perror("calloc(req_ps_sent)");
		free(c.fdtab);
		close(c.epfd);
		free(c.lat_us);
		free(c.req_ps_recv);
		return 1;
	}
	
	
	atomic_init(&c.lat_idx, 0);
	atomic_init(&c.inflight, 0);
	atomic_init(&c.sender_done, false);
	atomic_init(&c.stop, false);

	pthread_t ts, tr;
	if (pthread_create(&tr, NULL, receiver_thread, &c) != 0) {
		perror("pthread_create(receiver)");
		atomic_store(&c.stop, true);
		pthread_join(ts, NULL);
		return 1;
	}
	set_cpu_affinity(tr, 0);

	if (pthread_create(&ts, NULL, sender_thread, &c) != 0) {
		perror("pthread_create(sender)");
		return 1;
	}

	set_cpu_affinity(ts, 1);

	pthread_join(ts, NULL);
	pthread_join(tr, NULL);

	int n = atomic_load(&c.lat_idx);
	if (n > c.total_requests)
		n = c.total_requests;

	//print_stats(c.lat_us, n);
        //print_stats(c.lat_us, c.request_types, n);
        print_stats(c.lat_us,  n);


	char ts_s[100];
	char ts_r[100];

	//int pid = getpid();
	
	//sprintf(ts_s, "requests_sent_%d.txt", pid);
	sprintf(ts_s, "requests_sent_%drps-mod.txt", (int)c.avg_rps);
	//sprintf(ts_r, "requests_recv_%d.txt", pid);
	sprintf(ts_r, "requests_recv_%drps-mod.txt", (int)c.avg_rps);
	save_events_per_second(ts_s, c.req_ps_sent, c.total_requests);
	save_events_per_second(ts_r, c.req_ps_recv, c.total_requests);

	char latency[100];
	//sprintf(latency, "latencies_%d.txt", pid);
	sprintf(latency, "latencies_%drps-mod.txt", (int)c.avg_rps);
	//save_latencies(latency, c.lat_us, c.total_requests);
	save_latencies(latency, c.lat_us, c.total_requests, c.request_types);
	
	free(c.lat_us);
	free(c.fdtab);
	close(c.epfd);
	pthread_mutex_destroy(&c.fdtab_mu);

	return 0;
}



