/**
 * nnti_gni.c
 *
 *  Created on: Jan 13, 2011
 *      Author: thkorde
 */

#include "Trios_config.h"
#include "Trios_threads.h"
#include "Trios_timer.h"
#include "Trios_signal.h"
#include "Trios_nssi_fprint_types.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <alps/libalpslli.h>
#include <gni_pub.h>

#include <map>   // hash maps

#include "nnti_gni.h"
#include "nnti_utils.h"




#define USE_RDMA_EVENTS
#undef USE_RDMA_EVENTS

#define USE_FMA
//#define USE_RDMA
//#define USE_MIXED

/* mode 1 - client uses GNI params from ALPS to create a Commumnication Domain and attach to it */
//#define USE_ALPS_PTAG
/* mode 2 - client uses a mix of GNI params from ALPS and the server (cookie/pTag) to create a Commumnication Domain and attach to it */
#undef USE_ALPS_PTAG


#define NIC_ADDR_BITS    22
#define NIC_ADDR_SHIFT   (32-NIC_ADDR_BITS)
#define NIC_ADDR_MASK    0x3FFFFF
#define CPU_NUM_BITS     7
#define CPU_NUM_SHIFT    (NIC_ADDR_SHIFT-CPU_NUM_BITS)
#define CPU_NUM_MASK     0x7F
#define THREAD_NUM_BITS  3
#define THREAD_NUM_SHIFT (CPU_NUM_SHIFT-THREAD_NUM_BITS)
#define THREAD_NUM_MASK  0x7

#define GNI_INSTID(nic_addr, cpu_num, thr_num) (((nic_addr&NIC_ADDR_MASK)<<NIC_ADDR_SHIFT)|((cpu_num&CPU_NUM_MASK)<<CPU_NUM_SHIFT)|(thr_num&THREAD_NUM_MASK))
#define GNI_NIC_ADDRESS(inst_id)               ((inst_id>>NIC_ADDR_SHIFT)&NIC_ADDR_MASK)
#define GNI_CPU_NUMBER(inst_id)                ((inst_id>>CPU_NUM_SHIFT)&CPU_NUM_MASK)
#define GNI_THREAD_NUMBER(inst_id)             (inst_id&THREAD_NUM_MASK)



/**
 * These states are used to signal events between the completion handler
 * and the main client or server thread.
 *
 * Once CONNECTED, they cycle through RDMA_READ_ADV, RDMA_WRITE_ADV,
 * and RDMA_WRITE_COMPLETE for each ping.
 */
typedef enum {
    IDLE = 1,
    CONNECT_REQUEST,
    ADDR_RESOLVED,
    ROUTE_RESOLVED,
    CONNECTED,
    DISCONNECTED,
    ERROR
} gni_connection_state;

typedef enum {
    SERVER_CONNECTION,
    CLIENT_CONNECTION
} gni_connection_type;

typedef enum {
    REQUEST_BUFFER,
    RESULT_BUFFER,
    RECEIVE_BUFFER,
    SEND_BUFFER,
    GET_SRC_BUFFER,
    GET_DST_BUFFER,
    PUT_SRC_BUFFER,
    PUT_DST_BUFFER,
    RDMA_TARGET_BUFFER,
    UNKNOWN_BUFFER
} gni_buffer_type;


#define GNI_OP_PUT_INITIATOR  1
#define GNI_OP_GET_INITIATOR  2
#define GNI_OP_PUT_TARGET     3
#define GNI_OP_GET_TARGET     4
#define GNI_OP_SEND           5
#define GNI_OP_NEW_REQUEST    6
#define GNI_OP_RESULT         7
#define GNI_OP_RECEIVE        8

typedef enum {
    SEND_COMPLETE,
    RECV_COMPLETE,
    RDMA_WRITE_INIT,
    RDMA_WRITE_NEED_ACK,
    RDMA_WRITE_COMPLETE,
    RDMA_READ_INIT,
    RDMA_READ_NEED_ACK,
    RDMA_READ_COMPLETE,
    RDMA_TARGET_INIT,
    RDMA_TARGET_NEED_ACK,
    RDMA_TARGET_COMPLETE,
    RDMA_COMPLETE
} gni_op_state_t;

#define SRQ_WQ_DEPTH 2048
#define CQ_WQ_DEPTH 128
#define ACK_PER_CONN 64

typedef struct {
    gni_cq_handle_t cq_hdl;
    gni_ep_handle_t ep_hdl;
} conn_ep;


typedef struct {
    uint64_t ack_received;
    uint64_t inst_id;
    uint64_t byte_len;
    uint64_t byte_offset;
    uint64_t src_offset;
    uint64_t dest_offset;
    uint16_t op;
} nnti_gni_work_completion;

/**
 * attrs to send to the client
 */
typedef struct {
    uint64_t          req_index_addr; /* address of the request index var on the server */
    gni_mem_handle_t  req_index_mem_hdl;

    uint64_t         req_buffer_addr; /* address of the request buffer */
    uint64_t         req_size;        /* the maximum size of a request */
    uint64_t         req_count;       /* the number of requests that will fit in the queue */
    gni_mem_handle_t req_mem_hdl;

    uint64_t         wc_buffer_addr;  /* address of the work completion array */
    gni_mem_handle_t wc_mem_hdl;
} nnti_gni_server_queue_attrs;

/**
 * attrs to send to the server
 */
typedef struct {
    uint64_t         unblock_buffer_addr; /* address of the unblock buffer */
    gni_mem_handle_t unblock_mem_hdl;     /* mem hdl to send to the server */
} nnti_gni_client_queue_attrs;

typedef union {
    nnti_gni_client_queue_attrs client;
    nnti_gni_server_queue_attrs server;
} nnti_gni_queue_remote_attrs;


typedef struct {
    uint64_t         req_index;      /* after AMO Fetch-Add, this buffer contains the current index of the request queue */
    uint64_t         req_index_addr; /* address of the request index var on the client. */
    gni_cq_handle_t  req_index_mem_cq_hdl;
    gni_mem_handle_t req_index_mem_hdl;
    gni_cq_handle_t  req_index_cq_hdl;
    gni_ep_handle_t  req_index_ep_hdl;

    gni_cq_handle_t  req_cq_hdl;
    gni_ep_handle_t  req_ep_hdl;

    uint64_t         unblock_buffer;      /* small buffer to receive unblock messages */
    uint64_t         unblock_buffer_addr; /* address of the unblock buffer */
    gni_cq_handle_t  unblock_mem_cq_hdl;  /* CQ to wait for unblock events */
    gni_mem_handle_t unblock_mem_hdl;     /* mem hdl to send to the server.  the server uses this mem hdl as the
                                           * remote hdl when posting (CQ write) the unblock message. */
} nnti_gni_client_queue;

typedef struct {
    char             *peer_name;
    NNTI_ip_addr      peer_addr;
    NNTI_tcp_port     peer_port;
    uint32_t          peer_cookie;
    uint32_t          peer_ptag;
    NNTI_instance_id  peer_instance;

    alpsAppGni_t      peer_alps_info;

    nnti_gni_client_queue       queue_local_attrs;
    nnti_gni_queue_remote_attrs queue_remote_attrs;

    gni_cdm_handle_t     cdm_hdl;  /* a client creates this comm domain with params from the server */
    gni_nic_handle_t     nic_hdl;

    gni_connection_state state;

    gni_connection_type connection_type;
} gni_connection;

typedef struct {
    gni_buffer_type type;

    gni_connection *conn;

    gni_cq_handle_t        ep_cq_hdl;
    gni_ep_handle_t        ep_hdl;
    gni_cq_handle_t        mem_cq_hdl;
    gni_mem_handle_t       mem_hdl;
    gni_post_descriptor_t  post_desc;
    gni_post_descriptor_t *post_desc_ptr;

    nnti_gni_work_completion wc;
    gni_cq_handle_t          wc_cq_hdl;
    gni_ep_handle_t          wc_ep_hdl;
    gni_cq_handle_t          wc_mem_cq_hdl;
    gni_mem_handle_t         wc_mem_hdl;
    uint64_t                 wc_dest_addr;
    gni_mem_handle_t         wc_dest_mem_hdl;
    gni_post_descriptor_t    wc_post_desc;

    uint8_t          last_op;
    gni_op_state_t   op_state;
    uint8_t          is_last_op_complete;
} gni_memory_handle;

typedef struct {
    NNTI_buffer_t *reg_buf;

    uint64_t          last_index_before_reset;
    uint64_t          req_index;      /* index of the next available slot in the request queue */
    uint64_t          req_index_addr; /* address of the request index var on the server */
    gni_cq_handle_t   req_index_mem_cq_hdl;
    gni_mem_handle_t  req_index_mem_hdl;

    char            *req_buffer;      /* pointer to the head of the request buffer */
    uint64_t         req_size;        /* the maximum size of a request */
    uint64_t         req_count;       /* the number of requests that will fit in the queue */
    uint64_t         req_buffer_size; /* the size of the request buffer in bytes (req_size*req_count) */

    nnti_gni_work_completion *wc_buffer;      /* pointer to the head of the work completion array */
    uint64_t                  wc_buffer_size; /* the size of the work completion buffer in bytes (sizeof(nnti_gni_work_completion)*req_count) */
    gni_cq_handle_t           wc_mem_cq_hdl;
    gni_mem_handle_t          wc_mem_hdl;

    gni_cq_handle_t   unblock_cq_hdl;
    gni_ep_handle_t   unblock_ep_hdl;

    uint64_t req_processed_reset_limit;
    uint64_t req_processed;
    uint64_t total_req_processed;

    int req_recvd_count; /* number of requests received */
    int cqe_recvd_count; /* number of completion queue events received */
    int cqe_ack_count;   /* number of completion queue events acked */
} gni_request_queue_handle;

typedef struct {
    uint16_t delivery_mode;

    gni_cdm_handle_t     cdm_hdl;
    gni_nic_handle_t     nic_hdl;
    NNTI_instance_id     instance;

    gni_cq_handle_t      req_cq_hdl;

    uint64_t     apid;
    alpsAppGni_t alps_info;

    int      listen_sock;
    char     listen_name[NNTI_HOSTNAME_LEN];
    uint32_t listen_addr;  /* in NBO */
    uint16_t listen_port;  /* in NBO */

    gni_request_queue_handle req_queue;
} gni_transport_global;




static nthread_mutex_t nnti_gni_lock;


static NNTI_result_t register_memory(
        gni_memory_handle *hdl,
        void *buf,
        uint64_t len);
static NNTI_result_t unregister_memory(
        gni_memory_handle *hdl);
static gni_cq_handle_t get_cq(
        const NNTI_buffer_t *reg_buf);
static int process_event(
        const NNTI_buffer_t      *reg_buf,
        const NNTI_buf_ops_t      remote_op,
        gni_cq_handle_t           cq_hdl,
        gni_cq_entry_t           *ev_data,
        nnti_gni_work_completion *wc);
static int8_t is_buf_op_complete(
        const NNTI_buffer_t *reg_buf,
        const NNTI_buf_ops_t  remote_op);
static void create_peer(NNTI_peer_t *peer,
        char *name,
        NNTI_ip_addr addr,
        NNTI_tcp_port port,
        uint32_t ptag,
        uint32_t cookie,
        NNTI_instance_id instance);
static void copy_peer(
        NNTI_peer_t *src,
        NNTI_peer_t *dest);
static int init_server_listen_socket(void);
static int start_connection_listener_thread(void);
static uint32_t get_cpunum(void);
static void get_alps_info(alpsAppGni_t *alps_info);
static int tcp_read(int sock, void *incoming, size_t len);
static int tcp_write(int sock, const void *outgoing, size_t len);
static int tcp_exchange(int sock, int is_server, void *incoming, void *outgoing, size_t len);
static void transition_connection_to_ready(
        int sock,
        gni_connection *conn);
static NNTI_result_t get_ipaddr(
        char *ipaddr,
        int maxlen);
static NNTI_result_t init_connection(
        gni_connection **conn,
        const int sock,
        const char *peername,
        const NNTI_ip_addr  addr,
        const NNTI_tcp_port port,
        const int is_server);
static void close_connection(gni_connection *c);
static void print_wc(
        const nnti_gni_work_completion *wc);
static void print_cq_event(
        const gni_cq_entry_t *event);
static void print_post_desc(
        const gni_post_descriptor_t *post_desc_ptr);
static NNTI_result_t poll_cq(
        gni_cq_handle_t cq,
        int             timeout);
//static int32_t get_ack_index(gni_connection *conn);
//static void release_ack_index(gni_connection *conn, int32_t index);
static int need_mem_cq(const gni_memory_handle *gni_mem_hdl);
static void print_failed_cq(const NNTI_buffer_t *reg_buf);
static void print_gni_conn(gni_connection *c);
static NNTI_result_t insert_conn_peer(const NNTI_peer_t *peer, gni_connection *conn);
static NNTI_result_t insert_conn_instance(const NNTI_instance_id instance, gni_connection *conn);
static gni_connection *get_conn_peer(const NNTI_peer_t *peer);
static gni_connection *get_conn_instance(const NNTI_instance_id instance);
static gni_connection *del_conn_peer(const NNTI_peer_t *peer);
static gni_connection *del_conn_instance(const NNTI_instance_id instance);
static void close_all_conn(void);
//static void print_put_buf(void *buf, uint32_t size);
static void print_raw_buf(void *buf, uint32_t size);

static int set_rdma_type(
        gni_post_descriptor_t *pd);
static uint16_t get_dlvr_mode_from_env();
static int set_dlvr_mode(
        gni_post_descriptor_t *pd);

static int server_req_queue_init(
        gni_request_queue_handle *q,
        char                     *buffer,
        uint64_t                  req_size,
        uint64_t                  req_count);
static int server_req_queue_destroy(
        gni_request_queue_handle *q);

static int client_req_queue_init(
        gni_connection *c);
static int client_req_queue_destroy(
        gni_connection *c);

static int send_unblock(
        gni_request_queue_handle *local_req_queue_attrs);
static int request_wait(
        gni_request_queue_handle *q,
        gni_cq_entry_t           *ev_data);
static int reset_req_index(
        gni_request_queue_handle  *req_queue_attrs);

static int fetch_add_buffer_offset(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     addend,
        uint64_t                    *prev_offset);
static int send_req(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     offset,
        const NNTI_buffer_t         *reg_buf);
static int send_wc(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     offset,
        const NNTI_buffer_t         *reg_buf);
static int send_cqwrite(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     offset);
static int request_send(
        nnti_gni_client_queue       *client_q,
        nnti_gni_server_queue_attrs *server_q,
        const NNTI_buffer_t         *reg_buf,
        int                          req_num);


static gni_transport_global transport_global_data;
static const int MIN_TIMEOUT = 100;  /* in milliseconds */

static log_level nnti_cq_debug_level;
static log_level nnti_event_debug_level;
static log_level nnti_ee_debug_level;



/**
 * This custom key is used to look up existing connections.
 */
struct addrport_key {
    NNTI_ip_addr    addr;       /* part1 of a compound key */
    NNTI_tcp_port   port;       /* part2 of a compound key */

    // Need this operators for the hash map
    bool operator<(const addrport_key &key1) const {
        return addr < key1.addr;
    }

    bool operator>(const addrport_key &key1) const {
        return addr > key1.addr;
    }
};

/*
 * We need a couple of maps to keep track of connections.  Servers need to find
 * connections by QP number when requests arrive.  Clients need to find connections
 * by peer address and port.  Setup those maps here.
 */
static std::map<addrport_key, gni_connection *> connections_by_peer;
typedef std::map<addrport_key, gni_connection *>::iterator conn_by_peer_iter_t;
typedef std::pair<addrport_key, gni_connection *> conn_by_peer_t;
static nthread_mutex_t nnti_conn_peer_lock;

static std::map<NNTI_instance_id, gni_connection *> connections_by_instance;
typedef std::map<NNTI_instance_id, gni_connection *>::iterator conn_by_inst_iter_t;
typedef std::pair<NNTI_instance_id, gni_connection *> conn_by_inst_t;
static nthread_mutex_t nnti_conn_instance_lock;

#if 0
typedef struct {
    NNTI_instance_id  instance;   /* this is the key */
    gni_connection   *conn;
    UT_hash_handle    hh;         /* makes this structure hashable */
} conn_instance;

typedef struct {
    addrport_key    addrport;
    gni_connection *conn;
    UT_hash_handle  hh;         /* makes this structure hashable */
} conn_addrport;
#endif



/**
 * @brief Initialize NNTI to use a specific transport.
 *
 * Enable the use of a particular transport by this process.  <tt>my_url</tt>
 * allows the process to have some control (if possible) over the
 * URL assigned for the transport.  For example, a Portals URL to put
 * might be "ptl://-1,128".  This would tell Portals to use the default
 * network ID, but use PID=128.  If the transport
 * can be initialized without this info (eg. a Portals client), <tt>my_url</tt> can
 * be NULL or empty.
 */
NNTI_result_t NNTI_gni_init (
        const NNTI_transport_id_t  trans_id,
        const char                *my_url,
        NNTI_transport_t          *trans_hdl)
{
    static int initialized=0;

    int rc=NNTI_OK;

    trios_declare_timer(call_time);

    int flags;

    char transport[NNTI_URL_LEN];
    char address[NNTI_URL_LEN];
    char memdesc[NNTI_URL_LEN];
    char *sep, *endptr;

    char hostname[NNTI_HOSTNAME_LEN];
    NNTI_ip_addr  addr;
    NNTI_tcp_port port;

    uint32_t nic_addr  =0;
    uint32_t cpu_num   =0;
    uint32_t thread_num=0;
    uint32_t gni_cpu_id=0;


    assert(trans_hdl);

    log_debug(nnti_ee_debug_level, "enter");

    initialized=0;
    log_debug(nnti_debug_level, "my_url=%s", my_url);
    log_debug(nnti_debug_level, "initialized=%d, FALSE==%d", (int)initialized, (int)FALSE);

    if (!initialized) {

        memset(&transport_global_data, 0, sizeof(gni_transport_global));

        nnti_cq_debug_level=nnti_debug_level;
        nnti_event_debug_level=nnti_debug_level;
        nnti_ee_debug_level=nnti_debug_level;

        nthread_mutex_init(&nnti_gni_lock, NTHREAD_MUTEX_NORMAL);

        // initialize the mutexes for the connection maps
        nthread_mutex_init(&nnti_conn_peer_lock, NTHREAD_MUTEX_NORMAL);
        nthread_mutex_init(&nnti_conn_instance_lock, NTHREAD_MUTEX_NORMAL);

        log_debug(nnti_debug_level, "my_url=%s", my_url);

        hostname[0]='\0';
        if (my_url != NULL) {
            if ((rc=nnti_url_get_transport(my_url, transport, NNTI_URL_LEN)) != NNTI_OK) {
                goto cleanup;
            }
            if (0!=strcmp(transport, "gni")) {
                rc=NNTI_EINVAL;
                goto cleanup;
            }

            if ((rc=nnti_url_get_address(my_url, address, NNTI_URL_LEN)) != NNTI_OK) {
                goto cleanup;
            }

//            if ((rc=nnti_url_get_memdesc(my_url, memdesc, NNTI_URL_LEN)) != NNTI_OK) {
//                return(rc);
//            }

            sep=strchr(address, ':');
            if (sep == address) {
                /* no hostname given; try gethostname */
                gethostname(hostname, NNTI_HOSTNAME_LEN);
            } else {
                strncpy(hostname, address, sep-address);
            }
            sep++;
            port=strtol(sep, &endptr, 0);
            if (endptr == sep) {
                /* no port given; use -1 */
                port=-1;
            }
        } else {
            rc=get_ipaddr(hostname, NNTI_HOSTNAME_LEN);
            if (rc != NNTI_OK) {
                log_error(nnti_debug_level, "could not find IP address to listen on");
                goto cleanup;
            }
//            gethostname(hostname, NNTI_HOSTNAME_LEN);
            port=-1;
        }
        strcpy(transport_global_data.listen_name, hostname);


        transport_global_data.delivery_mode = get_dlvr_mode_from_env();


        log_debug(nnti_debug_level, "initializing Gemini");

//        /* register trace groups (let someone else enable) */
//        trace_counter_gid = trace_get_gid(TRACE_RPC_COUNTER_GNAME);
//        trace_interval_gid = trace_get_gid(TRACE_RPC_INTERVAL_GNAME);


        trios_start_timer(call_time);
        get_alps_info(&transport_global_data.alps_info);
        trios_stop_timer("get_alps_info", call_time);

        trios_start_timer(call_time);
        rc=GNI_CdmGetNicAddress (transport_global_data.alps_info.device_id, &nic_addr, &gni_cpu_id);
        trios_stop_timer("CdmGetNicAddress", call_time);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CdmGetNicAddress() failed: %d", rc);
            if (rc==GNI_RC_NO_MATCH)
                rc=NNTI_EEXIST;
            else
                rc=NNTI_EINVAL;

            goto cleanup;
        }

        trios_start_timer(call_time);
        cpu_num = get_cpunum();
        trios_stop_timer("get_cpunum", call_time);

        transport_global_data.instance=GNI_INSTID(nic_addr, cpu_num, thread_num);
        log_debug(nnti_debug_level, "nic_addr(%llu), cpu_num(%llu), thread_num(%llu), inst_id(%llu), "
                "derived.nic_addr(%llu), derived.cpu_num(%llu), derived.thread_num(%llu)",
                (uint64_t)nic_addr, (uint64_t)cpu_num, (uint64_t)thread_num,
                (uint64_t)transport_global_data.instance,
                (uint64_t)GNI_NIC_ADDRESS(transport_global_data.instance),
                (uint64_t)GNI_CPU_NUMBER(transport_global_data.instance),
                (uint64_t)GNI_THREAD_NUMBER(transport_global_data.instance));

        log_debug(nnti_debug_level, "global_nic_hdl - host(%s) device_id(%llu) local_addr(%lld) cookie(%llu) ptag(%llu) "
                    "apid(%llu) inst_id(%llu) gni_nic_addr(%llu) gni_cpu_id(%llu) linux_cpu_num(%llu) omp_thread_num(%llu)",
                    transport_global_data.listen_name,
                    (unsigned long long)transport_global_data.alps_info.device_id,
                    (long long)transport_global_data.alps_info.local_addr,
                    (unsigned long long)transport_global_data.alps_info.cookie,
                    (unsigned long long)transport_global_data.alps_info.ptag,
                    (unsigned long long)transport_global_data.apid,
                    (unsigned long long)transport_global_data.instance,
                    (unsigned long long)nic_addr,
                    (unsigned long long)gni_cpu_id,
                    (unsigned long long)cpu_num,
                    (unsigned long long)thread_num);

        trios_start_timer(call_time);
        rc=GNI_CdmCreate(transport_global_data.instance,
                transport_global_data.alps_info.ptag,
                transport_global_data.alps_info.cookie,
                GNI_CDM_MODE_ERR_NO_KILL,
                &transport_global_data.cdm_hdl);
        trios_stop_timer("CdmCreate", call_time);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CdmCreate() failed: %d", rc);
            rc=NNTI_EINVAL;
            goto cleanup;
        }

        trios_start_timer(call_time);
        rc=GNI_CdmAttach (transport_global_data.cdm_hdl,
                transport_global_data.alps_info.device_id,
                (uint32_t*)&transport_global_data.alps_info.local_addr, /* ALPS and GNI disagree about the type of local_addr.  cast here. */
                &transport_global_data.nic_hdl);
        trios_stop_timer("CdmAttach", call_time);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CdmAttach() failed: %d", rc);
            if (rc==GNI_RC_PERMISSION_ERROR)
                rc=NNTI_EPERM;
            else
                rc=NNTI_EINVAL;

            goto cleanup;
        }

        trios_start_timer(call_time);
        init_server_listen_socket();
        trios_stop_timer("init_server_listen_socket", call_time);
        trios_start_timer(call_time);
        start_connection_listener_thread();
        trios_stop_timer("start_connection_listener_thread", call_time);

        if (logging_info(nnti_debug_level)) {
            fprintf(logger_get_file(), "Gemini Initialized: host(%s) port(%u)\n",
                    transport_global_data.listen_name,
                    ntohs(transport_global_data.listen_port));
        }

        trios_start_timer(call_time);
        create_peer(
                &trans_hdl->me,
                transport_global_data.listen_name,
                transport_global_data.listen_addr,
                transport_global_data.listen_port,
                transport_global_data.alps_info.ptag,
                transport_global_data.alps_info.cookie,
                transport_global_data.instance);
        trios_stop_timer("create_peer", call_time);

        initialized = TRUE;
    }

cleanup:
    log_debug(nnti_ee_debug_level, "exit");

    return((NNTI_result_t)rc);
}


/**
 * @brief Return the URL field of this transport.
 *
 * Return the URL field of this transport.  After initialization, the transport will
 * have a specific location on the network where peers can contact it.  The
 * transport will convert this location to a string that other instances of the
 * transport will recognize.
 *
 * URL format: "transport://address/memory_descriptor"
 *    - transport - (required) identifies how the URL should parsed
 *    - address   - (required) uniquely identifies a location on the network
 *                - ex. "ptl://nid:pid/", "gni://ip_addr:port", "luc://endpoint_id/"
 *    - memory_descriptor - (optional) transport-specific representation of RMA params
 */
NNTI_result_t NNTI_gni_get_url (
        const NNTI_transport_t *trans_hdl,
        char                   *url,
        const uint64_t          maxlen)
{
    NNTI_result_t rc=NNTI_OK;

    assert(trans_hdl);
    assert(url);
    assert(maxlen>0);

    log_debug(nnti_ee_debug_level, "enter");

    strncpy(url, trans_hdl->me.url, maxlen);
    url[maxlen-1]='\0';

    log_debug(nnti_ee_debug_level, "exit");

    return(rc);
}


/**
 * @brief Prepare for communication with the peer identified by <tt>url</tt>.
 *
 * Parse <tt>url</tt> in a transport specific way.  Perform any transport specific
 * actions necessary to begin communication with this peer.
 *
 *
 * Connectionless transport: parse and populate
 * Connected transport: parse, connection and populate
 *
 */
NNTI_result_t NNTI_gni_connect (
        const NNTI_transport_t *trans_hdl,
        const char             *url,
        const int               timeout,
        NNTI_peer_t            *peer_hdl)
{
    int rc=NNTI_OK;

    trios_declare_timer(call_time);

    char transport[NNTI_URL_LEN];
    char address[NNTI_URL_LEN];
    char params[NNTI_URL_LEN];
    char *sep;

    char          hostname[NNTI_HOSTNAME_LEN];
    char          port_str[NNTI_HOSTNAME_LEN];
    NNTI_tcp_port port;

    char     *cookie_str;
    char     *ptag_str;

    int s;
    struct hostent *host_entry;
    struct sockaddr_in skin;
    socklen_t skin_size=sizeof(struct sockaddr_in);

    gni_connection *conn=NULL;

    NNTI_peer_t *key;

    double start_time;
    uint64_t elapsed_time = 0;
    int timeout_per_call;


    assert(trans_hdl);
    assert(peer_hdl);

    log_debug(nnti_ee_debug_level, "enter (url=%s)", url);

    conn = (gni_connection *)calloc(1, sizeof(gni_connection));
    log_debug(nnti_debug_level, "calloc returned conn=%p.", conn);
    if (conn == NULL) {
        log_error(nnti_debug_level, "calloc returned NULL.  out of memory?: %s", strerror(errno));
        rc=NNTI_ENOMEM;
        goto cleanup;
    }

    if (url != NULL) {
        if ((rc=nnti_url_get_transport(url, transport, NNTI_URL_LEN)) != NNTI_OK) {
            goto cleanup;
        }
        if (0!=strcmp(transport, "gni")) {
            /* the peer described by 'url' is not an Gemini peer */
            rc=NNTI_EINVAL;
            goto cleanup;
        }

        if ((rc=nnti_url_get_address(url, address, NNTI_URL_LEN)) != NNTI_OK) {
            /* the peer described by 'url' is not an Gemini peer */
            rc=NNTI_EINVAL;
            goto cleanup;
        }

        if ((rc=nnti_url_get_params(url, params, NNTI_URL_LEN)) != NNTI_OK) {
            /* the peer described by 'url' is not an Gemini peer */
            rc=NNTI_EINVAL;
            goto cleanup;
        }

        sep=strchr(address, ':');
        strncpy(hostname, address, sep-address);
        hostname[sep-address]='\0';
        strcpy(port_str, sep+1);
        port=strtol(port_str, NULL, 0);

        log_debug(nnti_ee_debug_level, "params=%s", params);

        ptag_str=strstr(params, "ptag=");
        sep=strchr(ptag_str, '&');
        *sep='\0';
        log_debug(nnti_ee_debug_level, "ptag_str=%s", ptag_str+5);
        conn->peer_ptag=strtol(ptag_str+5, NULL, 10);
        *sep='&';

        cookie_str=strstr(params, "cookie=");
        log_debug(nnti_ee_debug_level, "cookie_str=%s", cookie_str+7);
        conn->peer_cookie=strtoull(cookie_str+7, NULL, 10);

        log_debug(nnti_ee_debug_level, "url=%s", url);

    } else {
        /*  */
        rc=NNTI_EINVAL;
        goto cleanup;
    }

#ifdef USE_ALPS_PTAG
    conn->cdm_hdl = transport_global_data.cdm_hdl;
    conn->nic_hdl = transport_global_data.nic_hdl;
#else
    log_debug(nnti_ee_debug_level, "conn_nic_hdl - host(%s) device_id(%llu) local_addr(%lld) cookie(%llu) ptag(%llu) "
                "apid(%llu) inst_id(%llu) gni_nic_addr(%llu) linux_cpu_num(%llu) omp_thread_num(%llu)",
                transport_global_data.listen_name,
                (unsigned long long)transport_global_data.alps_info.device_id,
                (long long)transport_global_data.alps_info.local_addr,
                (unsigned long long)conn->peer_cookie,
                (unsigned long long)conn->peer_ptag,
                (unsigned long long)transport_global_data.apid,
                (unsigned long long)transport_global_data.instance,
                (uint64_t)GNI_NIC_ADDRESS(transport_global_data.instance),
                (uint64_t)GNI_CPU_NUMBER(transport_global_data.instance),
                (uint64_t)GNI_THREAD_NUMBER(transport_global_data.instance));

    trios_start_timer(call_time);
    rc=GNI_CdmCreate(transport_global_data.instance,
            conn->peer_ptag,
            conn->peer_cookie,
            GNI_CDM_MODE_ERR_ALL_KILL,
            &conn->cdm_hdl);
    trios_stop_timer("CdmCreate", call_time);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "CdmCreate() failed: %d", rc);
        rc=NNTI_EINVAL;
        goto cleanup;
    }

    trios_start_timer(call_time);
    rc=GNI_CdmAttach(conn->cdm_hdl,
            transport_global_data.alps_info.device_id,
            (uint32_t*)&transport_global_data.alps_info.local_addr, /* ALPS and GNI disagree about the type of local_addr.  cast here. */
            &conn->nic_hdl);
    trios_stop_timer("CdmAttach", call_time);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "CdmAttach() failed: %d", rc);
        if (rc==GNI_RC_PERMISSION_ERROR)
            rc=NNTI_EPERM;
        else
            rc=NNTI_EINVAL;
        goto cleanup;
    }
#endif



    host_entry = gethostbyname(hostname);
    if (!host_entry) {
        log_warn(nnti_debug_level, "failed to resolve server name (%s): %s", hostname, strerror(errno));
        rc=NNTI_ENOENT;
        goto cleanup;
    }
    memset(&skin, 0, sizeof(skin));
    skin.sin_family = host_entry->h_addrtype;
    memcpy(&skin.sin_addr, host_entry->h_addr_list[0], (size_t) host_entry->h_length);
    skin.sin_port = htons(port);

    elapsed_time=0;
    timeout_per_call = MIN_TIMEOUT;
//    if (timeout < 0)
//        timeout_per_call = MIN_TIMEOUT;
//    else
//        timeout_per_call = (timeout < MIN_TIMEOUT)? MIN_TIMEOUT : timeout;


    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        log_warn(nnti_debug_level, "failed to create tcp socket: errno=%d (%s)", errno, strerror(errno));
        rc=NNTI_EIO;
        goto cleanup;
    }
    trios_start_timer(call_time);
    start_time=trios_get_time();
    while((timeout==-1) || (elapsed_time < timeout)) {
        log_debug(nnti_debug_level, "calling connect");
        if (connect(s, (struct sockaddr *)&skin, skin_size) == 0) {
            log_debug(nnti_debug_level, "connected");
            break;
        }
        elapsed_time=(uint64_t)((trios_get_time()-start_time)*1000.0);
        log_warn(nnti_debug_level, "failed to connect to server (%s:%u): errno=%d (%s)", hostname, port, errno, strerror(errno));
        if ((timeout>0) && (elapsed_time >= timeout)) {
            rc=NNTI_EIO;
            goto cleanup;
        }
        nnti_sleep(timeout_per_call);
    }
    trios_stop_timer("socket connect", call_time);

    trios_start_timer(call_time);
    rc = init_connection(&conn, s, hostname, skin.sin_addr.s_addr, port, 0);
    trios_stop_timer("gni init connection", call_time);
    if (conn==NULL) {
        rc=NNTI_EIO;
        goto cleanup;
    }

    create_peer(
            peer_hdl,
            hostname,
            skin.sin_addr.s_addr,
            skin.sin_port,
            conn->peer_ptag,
            conn->peer_cookie,
            conn->peer_instance);

    key=(NNTI_peer_t *)malloc(sizeof(NNTI_peer_t));
    copy_peer(peer_hdl, key);
    insert_conn_peer(key, conn);

    transition_connection_to_ready(s, conn);

    if (close(s) < 0) {
        log_warn(nnti_debug_level, "failed to close tcp socket: errno=%d (%s)", errno, strerror(errno));
        rc=NNTI_EIO;
        goto cleanup;
    }

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_peer(logger_get_file(), "peer_hdl",
                "end of NNTI_gni_connect", peer_hdl);
    }

cleanup:
    if (rc != NNTI_OK) {
        if (conn!=NULL) free(conn);
    }
    log_debug(nnti_ee_debug_level, "exit");

    return((NNTI_result_t)rc);
}


/**
 * @brief Terminate communication with this peer.
 *
 * Perform any transport specific actions necessary to end communication with
 * this peer.
 */
NNTI_result_t NNTI_gni_disconnect (
        const NNTI_transport_t *trans_hdl,
        NNTI_peer_t            *peer_hdl)
{
    NNTI_result_t rc=NNTI_OK;

    assert(trans_hdl);
    assert(peer_hdl);

    log_debug(nnti_ee_debug_level, "enter");

    gni_connection *conn=get_conn_peer(peer_hdl);
    close_connection(conn);
    del_conn_peer(peer_hdl);

    free(peer_hdl->url);

    log_debug(nnti_ee_debug_level, "exit");

    return(rc);
}


/**
 * @brief Prepare a block of memory for network operations.
 *
 * Wrap a user allocated block of memory in an NNTI_buffer_t.  The transport
 * may take additional actions to prepare the memory for network send/receive.
 * If the memory block doesn't meet the transport's requirements for memory
 * regions, then errors or poor performance may result.
 */
NNTI_result_t NNTI_gni_register_memory (
        const NNTI_transport_t *trans_hdl,
        char                   *buffer,
        const uint64_t          size,
        const NNTI_buf_ops_t    ops,
        const NNTI_peer_t      *peer,
        NNTI_buffer_t          *reg_buf)
{
    NNTI_result_t rc=NNTI_OK;
    uint32_t i;
    trios_declare_timer(call_time);

    uint32_t cqe_num;


    struct ibv_recv_wr *bad_wr=NULL;

    gni_memory_handle *gni_mem_hdl=NULL;

    assert(trans_hdl);
    assert(buffer);
    assert(size>0);
    assert(ops>0);
    assert(reg_buf);

    log_debug(nnti_ee_debug_level, "enter");

    //    if (ops==NNTI_PUT_SRC) print_put_buf(buffer, size);

    gni_mem_hdl=(gni_memory_handle *)calloc(1, sizeof(gni_memory_handle));

    assert(gni_mem_hdl);

    reg_buf->transport_id      = trans_hdl->id;
    reg_buf->buffer_owner      = trans_hdl->me;
    reg_buf->ops               = ops;
    reg_buf->payload_size      = size;
    reg_buf->payload           = (uint64_t)buffer;
    reg_buf->transport_private = (uint64_t)gni_mem_hdl;
    if (peer != NULL) {
        reg_buf->peer = *peer;
    } else {
        GNI_SET_MATCH_ANY(&reg_buf->peer);
    }

    memset(&gni_mem_hdl->op_state, 0, sizeof(gni_op_state_t));

    log_debug(nnti_debug_level, "rpc_buffer->payload_size=%ld",
            reg_buf->payload_size);

    if (ops == NNTI_RECV_DST) {
        if ((size > NNTI_REQUEST_BUFFER_SIZE) && (size%NNTI_REQUEST_BUFFER_SIZE) == 0) {
            gni_request_queue_handle *q_hdl=&transport_global_data.req_queue;

            /*
             * This is a receive-only buffer.  This buffer is divisible by
             * NNTI_REQUEST_BUFFER_SIZE.  This buffer can hold more than
             * one short request.  Assume this buffer is a request queue.
             */
            gni_mem_hdl->type   =REQUEST_BUFFER;
            gni_mem_hdl->last_op=GNI_OP_NEW_REQUEST;

            memset(q_hdl, 0, sizeof(gni_request_queue_handle));

            q_hdl->reg_buf=reg_buf;

            server_req_queue_init(
                    q_hdl,
                    buffer,
                    NNTI_REQUEST_BUFFER_SIZE,
                    size/NNTI_REQUEST_BUFFER_SIZE);

            reg_buf->payload_size=q_hdl->req_size;

        } else if (size == NNTI_RESULT_BUFFER_SIZE) {
            /*
             * This is a receive-only buffer.  This buffer can hold exactly
             * one short result.  Assume this buffer is a result queue.
             */
            gni_mem_hdl->type    =RESULT_BUFFER;
            gni_mem_hdl->op_state=RDMA_WRITE_INIT;

            gni_mem_hdl->conn=get_conn_peer(peer);

            rc=register_memory(gni_mem_hdl, buffer, NNTI_RESULT_BUFFER_SIZE);

            print_gni_conn(gni_mem_hdl->conn);

        } else {
            /*
             * This is a receive-only buffer.  This buffer doesn't look
             * like a request buffer or a result buffer.  I don't know
             * what it is.  Assume it is a regular data buffer.
             */
            gni_mem_hdl->type    =RECEIVE_BUFFER;
            gni_mem_hdl->op_state=RDMA_WRITE_INIT;

            gni_mem_hdl->conn=get_conn_peer(peer);

            rc=register_memory(gni_mem_hdl, buffer, size);

        }

    } else if (ops == NNTI_SEND_SRC) {
        gni_mem_hdl->type=SEND_BUFFER;

        gni_mem_hdl->conn=get_conn_peer(peer);

        print_gni_conn(gni_mem_hdl->conn);

        rc=register_memory(gni_mem_hdl, buffer, size);

    } else if (ops == NNTI_GET_DST) {
        gni_mem_hdl->type    =GET_DST_BUFFER;
        gni_mem_hdl->op_state=RDMA_READ_INIT;

        gni_mem_hdl->conn=get_conn_peer(peer);

        rc=register_memory(gni_mem_hdl, buffer, size);

    } else if (ops == NNTI_GET_SRC) {
        gni_mem_hdl->type    =GET_SRC_BUFFER;
        gni_mem_hdl->op_state=RDMA_READ_INIT;

        gni_mem_hdl->conn=get_conn_peer(peer);

        rc=register_memory(gni_mem_hdl, buffer, size);

    } else if (ops == NNTI_PUT_SRC) {
//        print_put_buf(buffer, size);

        gni_mem_hdl->type    =PUT_SRC_BUFFER;
        gni_mem_hdl->op_state=RDMA_WRITE_INIT;

        gni_mem_hdl->conn=get_conn_peer(peer);

        rc=register_memory(gni_mem_hdl, buffer, size);

//        print_put_buf(buffer, size);

    } else if (ops == NNTI_PUT_DST) {
        gni_mem_hdl->type    =PUT_DST_BUFFER;
        gni_mem_hdl->op_state=RDMA_WRITE_INIT;

        gni_mem_hdl->conn=get_conn_peer(peer);

        rc=register_memory(gni_mem_hdl, buffer, size);

    } else if (ops == (NNTI_GET_SRC|NNTI_PUT_DST)) {
        gni_mem_hdl->type    =RDMA_TARGET_BUFFER;
        gni_mem_hdl->op_state=RDMA_TARGET_INIT;

        gni_mem_hdl->conn=get_conn_peer(peer);

        rc=register_memory(gni_mem_hdl, buffer, size);

    } else {
        gni_mem_hdl->type=UNKNOWN_BUFFER;
    }

    if (rc==NNTI_OK) {
        reg_buf->buffer_addr.transport_id                            = NNTI_TRANSPORT_GEMINI;
        reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword1 = gni_mem_hdl->mem_hdl.qword1;
        reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword2 = gni_mem_hdl->mem_hdl.qword2;

        if (gni_mem_hdl->type==REQUEST_BUFFER) {
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.size = transport_global_data.req_queue.req_size;
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.buf  = (uint64_t)transport_global_data.req_queue.req_buffer;
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.type = NNTI_GNI_REQUEST_BUFFER;
        } else {
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.size = reg_buf->payload_size;
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.buf  = (uint64_t)reg_buf->payload;
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.type = NNTI_GNI_SEND_SRC;

            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.wc_addr           = (uint64_t)&gni_mem_hdl->wc;
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.wc_mem_hdl.qword1 = gni_mem_hdl->wc_mem_hdl.qword1;
            reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.wc_mem_hdl.qword2 = gni_mem_hdl->wc_mem_hdl.qword2;
        }
    }

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_buffer(logger_get_file(), "reg_buf",
                "end of NNTI_gni_register_memory", reg_buf);
    }

cleanup:
    log_debug(nnti_ee_debug_level, "exit");
    return(rc);
}


/**
 * @brief Cleanup after network operations are complete.
 *
 * Destroy an NNTI_buffer_t that was previously created by NNTI_regsiter_buffer().
 * It is the user's responsibility to release the the memory region.
 */
NNTI_result_t NNTI_gni_unregister_memory (
        NNTI_buffer_t    *reg_buf)
{
    NNTI_result_t rc=NNTI_OK, rc2=NNTI_OK;
    gni_memory_handle *gni_mem_hdl=NULL;

    assert(reg_buf);

    log_debug(nnti_ee_debug_level, "enter");

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_buffer(logger_get_file(), "reg_buf",
                "start of NNTI_gni_unregister_memory", reg_buf);
    }

    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    assert(gni_mem_hdl);

    if (gni_mem_hdl->type==REQUEST_BUFFER) {
        server_req_queue_destroy(
                &transport_global_data.req_queue);

    } else {
        unregister_memory(gni_mem_hdl);
    }

cleanup:
    reg_buf->transport_id      = NNTI_TRANSPORT_NULL;
    GNI_SET_MATCH_ANY(&reg_buf->buffer_owner);
    reg_buf->ops               = (NNTI_buf_ops_t)0;
    GNI_SET_MATCH_ANY(&reg_buf->peer);
    reg_buf->payload_size      = 0;
    reg_buf->payload           = 0;
    reg_buf->transport_private = 0;

    log_debug(nnti_ee_debug_level, "exit");

    return(rc);
}


/**
 * @brief Send a message to a peer.
 *
 * Send a message (<tt>msg_hdl</tt>) to a peer (<tt>peer_hdl</tt>).  It is expected that the
 * message is small, but the exact maximum size is transport dependent.
 */
NNTI_result_t NNTI_gni_send (
        const NNTI_peer_t   *peer_hdl,
        const NNTI_buffer_t *msg_hdl,
        const NNTI_buffer_t *dest_hdl)
{
    NNTI_result_t rc=NNTI_OK;

    trios_declare_timer(call_time);

    gni_memory_handle *gni_mem_hdl=NULL;

    log_debug(nnti_ee_debug_level, "enter");

    assert(peer_hdl);
    assert(msg_hdl);

    gni_mem_hdl=(gni_memory_handle *)msg_hdl->transport_private;
    assert(gni_mem_hdl);

    if ((dest_hdl == NULL) || (dest_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.type == NNTI_GNI_REQUEST_BUFFER)) {
        gni_connection *conn=gni_mem_hdl->conn;
        assert(conn);

        trios_start_timer(call_time);
        request_send(&conn->queue_local_attrs, &conn->queue_remote_attrs.server, msg_hdl, 0);
        trios_stop_timer("send to request queue", call_time);
        gni_mem_hdl->last_op=GNI_OP_SEND;

    } else {
        trios_start_timer(call_time);
        rc=NNTI_gni_put(
                msg_hdl,
                0,
                msg_hdl->payload_size,
                dest_hdl,
                0);
        trios_stop_timer("send with put", call_time);
        if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "Put() failed: %d", rc);
        gni_mem_hdl->last_op=GNI_OP_PUT_INITIATOR;
    }

    log_debug(nnti_debug_level, "sending to (%s, ep=%llu, instance=%lu)", peer_hdl->url, gni_mem_hdl->ep_hdl, gni_mem_hdl->conn->peer_instance);

    log_debug(nnti_ee_debug_level, "exit");

    return(rc);
}


/**
 * @brief Transfer data to a peer.
 *
 * Put the contents of <tt>src_buffer_hdl</tt> into <tt>dest_buffer_hdl</tt>.  It is
 * assumed that the destination is at least <tt>src_length</tt> bytes in size.
 *
 */
NNTI_result_t NNTI_gni_put (
        const NNTI_buffer_t *src_buffer_hdl,
        const uint64_t       src_offset,
        const uint64_t       src_length,
        const NNTI_buffer_t *dest_buffer_hdl,
        const uint64_t       dest_offset)
{
    int rc=NNTI_OK;
    trios_declare_timer(call_time);

    struct ibv_send_wr *bad_wr=NULL;

    gni_memory_handle *gni_mem_hdl=NULL;

    log_debug(nnti_ee_debug_level, "enter");

    assert(src_buffer_hdl);
    assert(dest_buffer_hdl);


    gni_mem_hdl=(gni_memory_handle *)src_buffer_hdl->transport_private;

    memset(&gni_mem_hdl->post_desc, 0, sizeof(gni_post_descriptor_t));
#if defined(USE_RDMA) || defined(USE_MIXED)
    gni_mem_hdl->post_desc.type                  =GNI_POST_RDMA_PUT;
#if defined(USE_RDMA_EVENTS)
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_LOCAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#else
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_LOCAL_EVENT;
#endif
#elif defined(USE_FMA)
    gni_mem_hdl->post_desc.type                  =GNI_POST_FMA_PUT;
#if defined(USE_RDMA_EVENTS)
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_GLOBAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#else
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_GLOBAL_EVENT;
#endif
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    set_dlvr_mode(&gni_mem_hdl->post_desc);

    gni_mem_hdl->post_desc.local_addr            =src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.buf+src_offset;
    gni_mem_hdl->post_desc.local_mem_hndl.qword1 =src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword1;
    gni_mem_hdl->post_desc.local_mem_hndl.qword2 =src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword2;
    gni_mem_hdl->post_desc.remote_addr           =dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.buf+dest_offset;
    gni_mem_hdl->post_desc.remote_mem_hndl.qword1=dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword1;
    gni_mem_hdl->post_desc.remote_mem_hndl.qword2=dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword2;
    gni_mem_hdl->post_desc.length                =src_length;


    gni_mem_hdl->op_state=RDMA_WRITE_INIT;

    gni_mem_hdl->wc.op         =GNI_OP_PUT_TARGET;
    gni_mem_hdl->wc.byte_len   =src_length;
    gni_mem_hdl->wc.src_offset =src_offset;
    gni_mem_hdl->wc.dest_offset=dest_offset;

    gni_mem_hdl->wc_dest_addr          =dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.wc_addr;
    gni_mem_hdl->wc_dest_mem_hdl.qword1=dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.wc_mem_hdl.qword1;
    gni_mem_hdl->wc_dest_mem_hdl.qword2=dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.wc_mem_hdl.qword2;

#if defined(USE_RDMA) || defined(USE_MIXED)
    log_debug(nnti_event_debug_level, "calling PostRdma(rdma put ; ep_hdl(%llu) cq_hdl(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
            gni_mem_hdl->ep_hdl, gni_mem_hdl->ep_cq_hdl,
            gni_mem_hdl->post_desc.local_mem_hndl.qword1, gni_mem_hdl->post_desc.local_mem_hndl.qword2,
            gni_mem_hdl->post_desc.remote_mem_hndl.qword1, gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
    trios_start_timer(call_time);
    rc=GNI_PostRdma(gni_mem_hdl->ep_hdl, &gni_mem_hdl->post_desc);
    trios_stop_timer("PostRdma put", call_time);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "failed to post RDMA: %s", strerror(errno));
        rc=NNTI_EIO;
    }
#elif defined(USE_FMA)
    log_debug(nnti_event_debug_level, "calling PostFma(fma put ; ep_hdl(%llu) cq_hdl(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
            gni_mem_hdl->ep_hdl, gni_mem_hdl->ep_cq_hdl,
            gni_mem_hdl->post_desc.local_mem_hndl.qword1, gni_mem_hdl->post_desc.local_mem_hndl.qword2,
            gni_mem_hdl->post_desc.remote_mem_hndl.qword1, gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
    trios_start_timer(call_time);
    rc=GNI_PostFma(gni_mem_hdl->ep_hdl, &gni_mem_hdl->post_desc);
    trios_stop_timer("PostFma put", call_time);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "failed to post FMA: %s", strerror(errno));
        rc=NNTI_EIO;
    }
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    gni_mem_hdl->last_op=GNI_OP_PUT_INITIATOR;

    log_debug(nnti_ee_debug_level, "exit");

    return((NNTI_result_t)rc);
}


/**
 * @brief Transfer data from a peer.
 *
 * Get the contents of <tt>src_buffer_hdl</tt> into <tt>dest_buffer_hdl</tt>.  It is
 * assumed that the destination is at least <tt>src_length</tt> bytes in size.
 *
 */
NNTI_result_t NNTI_gni_get (
        const NNTI_buffer_t *src_buffer_hdl,
        const uint64_t       src_offset,
        const uint64_t       src_length,
        const NNTI_buffer_t *dest_buffer_hdl,
        const uint64_t       dest_offset)
{
    int rc=NNTI_OK;
    trios_declare_timer(call_time);

    struct ibv_send_wr *bad_wr=NULL;

    gni_memory_handle *gni_mem_hdl=NULL;


    log_debug(nnti_ee_debug_level, "enter");

    assert(src_buffer_hdl);
    assert(dest_buffer_hdl);


    gni_mem_hdl=(gni_memory_handle *)dest_buffer_hdl->transport_private;

    memset(&gni_mem_hdl->post_desc, 0, sizeof(gni_post_descriptor_t));
#if defined(USE_RDMA) || defined(USE_MIXED)
    gni_mem_hdl->post_desc.type                  =GNI_POST_RDMA_GET;
#if defined(USE_RDMA_EVENTS)
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_LOCAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#else
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_LOCAL_EVENT;
#endif
#elif defined(USE_FMA)
    gni_mem_hdl->post_desc.type                  =GNI_POST_FMA_GET;
#if defined(USE_RDMA_EVENTS)
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_GLOBAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#else
    gni_mem_hdl->post_desc.cq_mode               =GNI_CQMODE_GLOBAL_EVENT;
#endif
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    set_dlvr_mode(&gni_mem_hdl->post_desc);

    gni_mem_hdl->post_desc.local_addr            =dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.buf+dest_offset;
    gni_mem_hdl->post_desc.local_mem_hndl.qword1 =dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword1;
    gni_mem_hdl->post_desc.local_mem_hndl.qword2 =dest_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword2;
    gni_mem_hdl->post_desc.remote_addr           =src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.buf+src_offset;
    gni_mem_hdl->post_desc.remote_mem_hndl.qword1=src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword1;
    gni_mem_hdl->post_desc.remote_mem_hndl.qword2=src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword2;
    gni_mem_hdl->post_desc.length                =src_length;

    gni_mem_hdl->op_state=RDMA_READ_INIT;

    gni_mem_hdl->wc.op         =GNI_OP_GET_TARGET;
    gni_mem_hdl->wc.byte_len   =src_length;
    gni_mem_hdl->wc.src_offset =src_offset;
    gni_mem_hdl->wc.dest_offset=dest_offset;

    gni_mem_hdl->wc_dest_addr          =src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.wc_addr;
    gni_mem_hdl->wc_dest_mem_hdl.qword1=src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.wc_mem_hdl.qword1;
    gni_mem_hdl->wc_dest_mem_hdl.qword2=src_buffer_hdl->buffer_addr.NNTI_remote_addr_t_u.gni.wc_mem_hdl.qword2;

#if defined(USE_RDMA) || defined(USE_MIXED)
    log_debug(nnti_event_debug_level, "calling PostRdma(rdma get ; ep_hdl(%llu) cq_hdl(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
            gni_mem_hdl->ep_hdl, gni_mem_hdl->ep_cq_hdl,
            gni_mem_hdl->post_desc.local_mem_hndl.qword1, gni_mem_hdl->post_desc.local_mem_hndl.qword2,
            gni_mem_hdl->post_desc.remote_mem_hndl.qword1, gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
    trios_start_timer(call_time);
    rc=GNI_PostRdma(gni_mem_hdl->ep_hdl, &gni_mem_hdl->post_desc);
    trios_stop_timer("PostRdma get", call_time);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "failed to post RDMA: %s", strerror(errno));
        rc=NNTI_EIO;
    }
#elif defined(USE_FMA)
    log_debug(nnti_event_debug_level, "calling PostFma(fma get ; ep_hdl(%llu) cq_hdl(%llu) local_mem_hdl(%llu, %llu) remote_mem_hdl(%llu, %llu))",
            gni_mem_hdl->ep_hdl, gni_mem_hdl->ep_cq_hdl,
            gni_mem_hdl->post_desc.local_mem_hndl.qword1, gni_mem_hdl->post_desc.local_mem_hndl.qword2,
            gni_mem_hdl->post_desc.remote_mem_hndl.qword1, gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
    trios_start_timer(call_time);
    rc=GNI_PostFma(gni_mem_hdl->ep_hdl, &gni_mem_hdl->post_desc);
    trios_stop_timer("PostFma get", call_time);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "failed to post FMA: %s", strerror(errno));
        rc=NNTI_EIO;
    }
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    gni_mem_hdl->last_op=GNI_OP_GET_INITIATOR;

    log_debug(nnti_ee_debug_level, "exit");

    return((NNTI_result_t)rc);
}


/**
 * @brief Wait for <tt>remote_op</tt> on <tt>reg_buf</tt> to complete.
 *
 * Wait for <tt>remote_op</tt> on <tt>reg_buf</tt> to complete or timeout
 * waiting.  This is typically used to wait for a result or a bulk data
 * transfer.  The timeout is specified in milliseconds.  A timeout of <tt>-1</tt>
 * means wait forever.  A timeout of <tt>0</tt> means do not wait.
 *
 */
NNTI_result_t NNTI_gni_wait (
        const NNTI_buffer_t  *reg_buf,
        const NNTI_buf_ops_t  remote_op,
        const int             timeout,
        NNTI_status_t        *status)
{
    int nnti_rc=NNTI_OK;
    gni_memory_handle        *gni_mem_hdl=NULL;
    gni_request_queue_handle *q_hdl=NULL;
    gni_connection           *conn=NULL;

    gni_cq_handle_t cq_hdl=0;
    gni_cq_entry_t  ev_data;

    uint8_t retry_count=0;

    nnti_gni_work_completion wc;

    NNTI_instance_id peer_instance=0;

    void *buf=NULL;

    gni_return_t rc=GNI_RC_SUCCESS;
    int elapsed_time = 0;
    int timeout_per_call;

    trios_declare_timer(call_time);

    assert(reg_buf);
    assert(status);

    log_debug(nnti_ee_debug_level, "enter");

    q_hdl      =&transport_global_data.req_queue;
    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    assert(q_hdl);
    assert(gni_mem_hdl);

    if (timeout < 0)
        timeout_per_call = MIN_TIMEOUT;
    else
        timeout_per_call = (timeout < MIN_TIMEOUT)? MIN_TIMEOUT : timeout;

    retry_count=0;
    while (1)   {
        if (trios_exit_now()) {
            log_debug(nnti_debug_level, "caught abort signal");
            nnti_rc=NNTI_ECANCELED;
            break;
        }

retry:
        cq_hdl=get_cq(reg_buf);

        if ((gni_mem_hdl->type == REQUEST_BUFFER) &&
            (q_hdl->wc_buffer[q_hdl->req_processed].ack_received==1)) {
            log_debug(nnti_event_debug_level, "processing out of order on cq_hdl(%llu) (l.qw1=%llu l.qw2=%llu r.qw1=%llu r.qw2=%llu)",
                    (uint64_t)cq_hdl,
                    (uint64_t)gni_mem_hdl->post_desc.local_mem_hndl.qword1,
                    (uint64_t)gni_mem_hdl->post_desc.local_mem_hndl.qword2,
                    (uint64_t)gni_mem_hdl->post_desc.remote_mem_hndl.qword1,
                    (uint64_t)gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
            memset(&ev_data, 0, sizeof(ev_data));
            memset(&wc, 0, sizeof(nnti_gni_work_completion));
            process_event(reg_buf, remote_op, cq_hdl, &ev_data, &wc);
            log_debug(nnti_event_debug_level, "out of order processing complete on cq_hdl(%llu) (l.qw1=%llu l.qw2=%llu r.qw1=%llu r.qw2=%llu)",
                    (uint64_t)cq_hdl,
                    (uint64_t)gni_mem_hdl->post_desc.local_mem_hndl.qword1,
                    (uint64_t)gni_mem_hdl->post_desc.local_mem_hndl.qword2,
                    (uint64_t)gni_mem_hdl->post_desc.remote_mem_hndl.qword1,
                    (uint64_t)gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
            if (is_buf_op_complete(reg_buf, remote_op) == TRUE) {
                nnti_rc = NNTI_OK;
                break;
            } else {
                continue;
            }
        } else {
            memset(&ev_data, 0, sizeof(ev_data));
            log_debug(nnti_event_debug_level, "calling CqWaitEvent(wait) on cq_hdl(%llu) (l.qw1=%llu l.qw2=%llu r.qw1=%llu r.qw2=%llu)",
                    (uint64_t)cq_hdl,
                    (uint64_t)gni_mem_hdl->post_desc.local_mem_hndl.qword1,
                    (uint64_t)gni_mem_hdl->post_desc.local_mem_hndl.qword2,
                    (uint64_t)gni_mem_hdl->post_desc.remote_mem_hndl.qword1,
                    (uint64_t)gni_mem_hdl->post_desc.remote_mem_hndl.qword2);
//            log_debug(nnti_event_debug_level, "calling CqWaitEvent(wait)");
            trios_start_timer(call_time);
//            nthread_lock(&nnti_gni_lock);
            rc=GNI_CqWaitEvent (cq_hdl, timeout_per_call, &ev_data);
//            nthread_unlock(&nnti_gni_lock);
            trios_stop_timer("NNTI_gni_wait - CqWaitEvent", call_time);
            log_debug(nnti_event_debug_level, "CqWaitEvent(wait) complete");
            if (rc!=GNI_RC_SUCCESS) log_debug(nnti_debug_level, "CqWaitEvent() on cq_hdl(%llu) failed: %d", (uint64_t)cq_hdl, rc);
        }

        /* case 1: success */
        if (rc == GNI_RC_SUCCESS) {
            nnti_rc = NNTI_OK;
        }
        /* case 2: timed out */
        else if (rc==GNI_RC_TIMEOUT) {
            elapsed_time += timeout_per_call;

            /* if the caller asked for a legitimate timeout, we need to exit */
            if (((timeout > 0) && (elapsed_time >= timeout)) || trios_exit_now()) {
                log_debug(nnti_debug_level, "CqWaitEvent timed out...timeout(%d) elapsed_time(%d) exit_now(%d)",
                        timeout, elapsed_time, trios_exit_now());
                nnti_rc = NNTI_ETIMEDOUT;
                break;
            }
            /* continue if the timeout has not expired */
            log_debug(nnti_event_debug_level, "CqWaitEvent timedout...retrying...timeout(%d) elapsed_time(%d) exit_now(%d)",
                        timeout, elapsed_time, trios_exit_now());
//            if (elapsed_time >= 500) {
//                fprint_NNTI_buffer(logger_get_file(), "reg_buf",
//                        "NNTI_wait() timeout(5+ sec)", reg_buf);
//            }

            nthread_yield();

//            goto retry;
            continue;
        }
        /* case 3: failure */
        else {
            char errstr[1024];
            uint32_t recoverable=111;
            GNI_CqErrorStr(ev_data, errstr, 1024);
            GNI_CqErrorRecoverable(ev_data, &recoverable);

            log_error(nnti_debug_level, "CqWaitEvent failed (cq_hdl=%llu ; rc=%d) (recoverable=%llu) : %s",
                    (uint64_t)cq_hdl, rc, (uint64_t)recoverable, errstr);
            print_failed_cq(reg_buf);
            fprint_NNTI_buffer(logger_get_file(), "reg_buf",
                            "NNTI_wait() error", reg_buf);
            nnti_rc = NNTI_EIO;
            break;
        }

        if (rc == GNI_RC_SUCCESS) {
            print_cq_event(&ev_data);

            if (!GNI_CQ_STATUS_OK(ev_data)) {
                char errstr[1024];
                GNI_CqErrorStr(ev_data, errstr, 1024);
                log_error(nnti_debug_level, "Failed status %s (%d)",
                        errstr,
                        GNI_CQ_GET_STATUS(ev_data));
                nnti_rc=NNTI_EIO;
                break;
            }

            memset(&wc, 0, sizeof(nnti_gni_work_completion));
            process_event(reg_buf, remote_op, cq_hdl, &ev_data, &wc);
            if (is_buf_op_complete(reg_buf, remote_op) == TRUE) {
                nnti_rc = NNTI_OK;
                break;
            }
        }
    }

    if (gni_mem_hdl->type == REQUEST_BUFFER) {
        conn = get_conn_instance(GNI_CQ_GET_INST_ID(ev_data));
    } else {
        conn = gni_mem_hdl->conn;
    }

    print_wc(&wc);
    if (nnti_rc == NNTI_OK) {
        print_raw_buf((char *)reg_buf->payload+wc.byte_offset, wc.byte_len);
    }

    memset(status, 0, sizeof(NNTI_status_t));
    status->op     = remote_op;
    status->result = (NNTI_result_t)nnti_rc;
    if (nnti_rc==NNTI_OK) {
        status->start  = (uint64_t)reg_buf->payload;
        status->offset = wc.byte_offset;
        status->length = wc.byte_len;
        switch (gni_mem_hdl->last_op) {
            case GNI_OP_PUT_INITIATOR:
            case GNI_OP_GET_TARGET:
            case GNI_OP_SEND:
                create_peer(&status->src,
                        transport_global_data.listen_name,
                        transport_global_data.listen_addr,
                        transport_global_data.listen_port,
                        transport_global_data.alps_info.ptag,
                        transport_global_data.alps_info.cookie,
                        transport_global_data.instance);
                create_peer(&status->dest,
                        conn->peer_name,
                        conn->peer_addr,
                        conn->peer_port,
                        conn->peer_ptag,
                        conn->peer_cookie,
                        conn->peer_instance);
                break;
            case GNI_OP_GET_INITIATOR:
            case GNI_OP_PUT_TARGET:
            case GNI_OP_NEW_REQUEST:
            case GNI_OP_RESULT:
            case GNI_OP_RECEIVE:
                create_peer(&status->src,
                        conn->peer_name,
                        conn->peer_addr,
                        conn->peer_port,
                        conn->peer_ptag,
                        conn->peer_cookie,
                        conn->peer_instance);
                create_peer(&status->dest,
                        transport_global_data.listen_name,
                        transport_global_data.listen_addr,
                        transport_global_data.listen_port,
                        transport_global_data.alps_info.ptag,
                        transport_global_data.alps_info.cookie,
                        transport_global_data.instance);
                break;
            }
    }

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_status(logger_get_file(), "status",
                "end of NNTI_wait", status);
    }

    if ((nnti_rc==NNTI_OK) && (gni_mem_hdl->type == REQUEST_BUFFER)) {
        gni_mem_hdl->op_state = (gni_op_state_t)0;
    }

cleanup:
    log_debug(nnti_ee_debug_level, "exit");
    return((NNTI_result_t)nnti_rc);
}


/**
 * @brief Disable this transport.
 *
 * Shutdown the transport.  Any outstanding sends, gets and puts will be
 * canceled.  Any new transport requests will fail.
 *
 */
NNTI_result_t NNTI_gni_fini (
        const NNTI_transport_t *trans_hdl)
{
    log_debug(nnti_ee_debug_level, "enter");
    close_all_conn();
    log_debug(nnti_ee_debug_level, "exit");

    return(NNTI_OK);
}

static NNTI_result_t register_memory(gni_memory_handle *hdl, void *buf, uint64_t len)
{
    int rc=GNI_RC_SUCCESS; /* return code */

    trios_declare_timer(call_time);

    gni_connection *conn=NULL;

    assert(hdl);

    conn=hdl->conn;
    assert(conn);

    hdl->mem_cq_hdl   =NULL;
    hdl->wc_mem_cq_hdl=NULL;


    log_debug(nnti_debug_level, "enter hdl(%p) buffer(%p) len(%d)", hdl, buf, len);

#if defined(USE_RDMA_EVENTS)
    if (need_mem_cq(hdl) == 1) {
        rc=GNI_CqCreate (conn->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &hdl->mem_cq_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CqCreate(mem_cq_hdl) failed: %d", rc);
            goto cleanup;
        }
    }
#endif
    rc=GNI_CqCreate (conn->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &hdl->ep_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "CqCreate(ep_cq_hdl) failed: %d", rc);
        goto cleanup;
    }
    rc=GNI_EpCreate (conn->nic_hdl, hdl->ep_cq_hdl, &hdl->ep_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpCreate(ep_hdl) failed: %d", rc);
        goto cleanup;
    }
    rc=GNI_EpBind (hdl->ep_hdl, hdl->conn->peer_alps_info.local_addr, hdl->conn->peer_instance);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpBind(ep_hdl) failed: %d", rc);
        goto cleanup;
    }

    trios_start_timer(call_time);
    rc=GNI_MemRegister (conn->nic_hdl,
            (uint64_t)buf,
            len,
            hdl->mem_cq_hdl,
            GNI_MEM_READWRITE,
            -1,
            &hdl->mem_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "MemRegister(mem_hdl) failed: rc=%d, %s", rc, strerror(errno));
        goto cleanup;
    }
    trios_stop_timer("buf register", call_time);

    if (need_mem_cq(hdl) == 1) {
        rc=GNI_CqCreate (conn->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &hdl->wc_mem_cq_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CqCreate(wc_mem_cq_hdl) failed: %d", rc);
            goto cleanup;
        }
    }
    rc=GNI_CqCreate (conn->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &hdl->wc_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "CqCreate(wc_cq_hdl) failed: %d", rc);
        goto cleanup;
    }
    rc=GNI_EpCreate (conn->nic_hdl, hdl->wc_cq_hdl, &hdl->wc_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpCreate(wc_ep_hdl) failed: %d", rc);
        goto cleanup;
    }
    rc=GNI_EpBind (hdl->wc_ep_hdl, hdl->conn->peer_alps_info.local_addr, hdl->conn->peer_instance);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpBind(ep_hdl) failed: %d", rc);
        goto cleanup;
    }

    trios_start_timer(call_time);
    rc=GNI_MemRegister (conn->nic_hdl,
            (uint64_t)&hdl->wc,
            sizeof(nnti_gni_work_completion),
            hdl->wc_mem_cq_hdl,
            GNI_MEM_READWRITE,
            -1,
            &hdl->wc_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "MemRegister(wc_mem_hdl) failed: rc=%d, %s", rc, strerror(errno));
        goto cleanup;
    }
    trios_stop_timer("wc register", call_time);

    log_debug(nnti_debug_level, "register hdl->ep_hdl    =%llu", (uint64_t)hdl->ep_hdl);
    log_debug(nnti_debug_level, "register hdl->ep_cq_hdl =%llu", (uint64_t)hdl->ep_cq_hdl);
    log_debug(nnti_debug_level, "register hdl->mem_cq_hdl=%llu", (uint64_t)hdl->mem_cq_hdl);
    log_debug(nnti_debug_level, "register hdl->mem_hdl   =(%llu,%llu)", (uint64_t)hdl->mem_hdl.qword1, (uint64_t)hdl->mem_hdl.qword2);
    log_debug(nnti_debug_level, "register hdl->wc_ep_hdl    =%llu", (uint64_t)hdl->wc_ep_hdl);
    log_debug(nnti_debug_level, "register hdl->wc_cq_hdl    =%llu", (uint64_t)hdl->wc_cq_hdl);
    log_debug(nnti_debug_level, "register hdl->wc_mem_cq_hdl=%llu", (uint64_t)hdl->wc_mem_cq_hdl);
    log_debug(nnti_debug_level, "register hdl->wc_mem_hdl   =(%llu,%llu)", (uint64_t)hdl->wc_mem_hdl.qword1, (uint64_t)hdl->wc_mem_hdl.qword2);


    log_debug(nnti_debug_level, "exit  hdl(%p) buffer(%p)", hdl, buf);

    return((NNTI_result_t)GNI_RC_SUCCESS);

cleanup:
//    unregister_memory(hdl);

    switch(rc) {
        case GNI_RC_SUCCESS:
            rc=(int)NNTI_OK;
        default:
            rc=(int)NNTI_EIO;
    }

    return ((NNTI_result_t)rc);
}

static NNTI_result_t unregister_memory(gni_memory_handle *hdl)
{
    int rc=GNI_RC_SUCCESS; /* return code */
    int i=0;
    trios_declare_timer(call_time);
    gni_cq_entry_t  ev_data;

    gni_connection *conn=NULL;

    assert(hdl);

    conn=hdl->conn;
    assert(conn);

    log_debug(nnti_debug_level, "enter hdl(%p)", hdl);

    log_debug(nnti_debug_level, "unregister hdl->ep_hdl    =%llu", (uint64_t)hdl->ep_hdl);
    log_debug(nnti_debug_level, "unregister hdl->ep_cq_hdl =%llu", (uint64_t)hdl->ep_cq_hdl);
    log_debug(nnti_debug_level, "unregister hdl->mem_cq_hdl=%llu", (uint64_t)hdl->mem_cq_hdl);
    log_debug(nnti_debug_level, "unregister hdl->mem_hdl   =(%llu,%llu)", (uint64_t)hdl->mem_hdl.qword1, (uint64_t)hdl->mem_hdl.qword2);
    log_debug(nnti_debug_level, "unregister hdl->wc_ep_hdl    =%llu", (uint64_t)hdl->wc_ep_hdl);
    log_debug(nnti_debug_level, "unregister hdl->wc_cq_hdl    =%llu", (uint64_t)hdl->wc_cq_hdl);
    log_debug(nnti_debug_level, "unregister hdl->wc_mem_cq_hdl=%llu", (uint64_t)hdl->wc_mem_cq_hdl);
    log_debug(nnti_debug_level, "unregister hdl->wc_mem_hdl   =(%llu,%llu)", (uint64_t)hdl->wc_mem_hdl.qword1, (uint64_t)hdl->wc_mem_hdl.qword2);

    trios_start_timer(call_time);
    rc=GNI_MemDeregister (conn->nic_hdl, &hdl->mem_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "MemDeregister(mem_hdl) failed: %d", rc);
    }
    trios_stop_timer("buf deregister", call_time);

    rc=GNI_EpUnbind (hdl->ep_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpUnbind(ep_hdl) failed: %d", rc);
    }
    rc=GNI_EpDestroy (hdl->ep_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpDestroy(ep_hdl) failed: %d", rc);
    }
    rc=GNI_CqDestroy (hdl->ep_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "CqDestroy(ep_cq_hdl) failed: %d", rc);
    }
#if defined(USE_RDMA_EVENTS)
    if (need_mem_cq(hdl) == 1) {
        rc=GNI_CqDestroy (hdl->mem_cq_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CqDestroy(mem_cq_hdl) failed: %d", rc);
        }
    }
#endif

    trios_start_timer(call_time);
    rc=GNI_MemDeregister (conn->nic_hdl, &hdl->wc_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "MemDeregister(wc_mem_hdl) failed: %d", rc);
    }
    trios_stop_timer("wc deregister", call_time);

    rc=GNI_EpUnbind (hdl->wc_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpUnbind(wc_ep_hdl) failed: %d", rc);
    }
    rc=GNI_EpDestroy (hdl->wc_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "EpDestroy(wc_ep_hdl) failed: %d", rc);
    }
    rc=GNI_CqDestroy (hdl->wc_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) {
        log_error(nnti_debug_level, "CqDestroy(wc_cq_hdl) failed: %d", rc);
    }
    if (need_mem_cq(hdl) == 1) {
        rc=GNI_CqDestroy (hdl->wc_mem_cq_hdl);
        if (rc!=GNI_RC_SUCCESS) {
            log_error(nnti_debug_level, "CqDestroy(wc_mem_cq_hdl) failed: %d", rc);
        }
    }

    switch(rc) {
        case GNI_RC_SUCCESS:
            rc=NNTI_OK;
        default:
            rc=NNTI_EIO;
    }

    log_debug(nnti_debug_level, "exit  hdl(%p)", hdl);

    return ((NNTI_result_t)rc);
}

static void send_ack (
        const NNTI_buffer_t *reg_buf)
{
    int rc=NNTI_OK;
    trios_declare_timer(call_time);

    gni_memory_handle *gni_mem_hdl=NULL;

    assert(reg_buf);

    log_debug(nnti_ee_debug_level, "enter");

    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    memset(&gni_mem_hdl->wc_post_desc, 0, sizeof(gni_post_descriptor_t));
#if defined(USE_FMA) || defined(USE_MIXED)
    gni_mem_hdl->wc_post_desc.type           =GNI_POST_FMA_PUT;
    gni_mem_hdl->wc_post_desc.cq_mode        =GNI_CQMODE_GLOBAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#elif defined(USE_RDMA)
    gni_mem_hdl->wc_post_desc.type           =GNI_POST_RDMA_PUT;
    gni_mem_hdl->wc_post_desc.cq_mode        =GNI_CQMODE_LOCAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    set_dlvr_mode(&gni_mem_hdl->post_desc);

    gni_mem_hdl->wc_post_desc.local_addr     =(uint64_t)&gni_mem_hdl->wc;
    gni_mem_hdl->wc_post_desc.local_mem_hndl =gni_mem_hdl->wc_mem_hdl;
    gni_mem_hdl->wc_post_desc.remote_addr    =gni_mem_hdl->wc_dest_addr;
    gni_mem_hdl->wc_post_desc.remote_mem_hndl=gni_mem_hdl->wc_dest_mem_hdl;
    gni_mem_hdl->wc_post_desc.length         =sizeof(nnti_gni_work_completion);

    log_debug(nnti_debug_level, "sending ACK to (%s, ep_hdl=%llu, peer_instance=%lu)",
            reg_buf->peer.url, gni_mem_hdl->wc_ep_hdl, gni_mem_hdl->conn->peer_instance);

#if defined(USE_FMA) || defined(USE_MIXED)
    log_debug(nnti_debug_level, "calling PostFma(fma wc wc_ep_hdl(%llu) wc_cq_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            gni_mem_hdl->wc_ep_hdl, gni_mem_hdl->wc_cq_hdl, gni_mem_hdl->wc_post_desc.local_addr, gni_mem_hdl->wc_post_desc.remote_addr);
    trios_start_timer(call_time);
    rc=GNI_PostFma(gni_mem_hdl->wc_ep_hdl, &gni_mem_hdl->wc_post_desc);
    trios_stop_timer("PostFma ack", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostFma(fma wc) failed: %d", rc);
#elif defined(USE_RDMA)
    log_debug(nnti_debug_level, "calling PostRdma(rdma wc wc_ep_hdl(%llu) wc_cq_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            gni_mem_hdl->wc_ep_hdl, gni_mem_hdl->wc_cq_hdl, gni_mem_hdl->wc_post_desc.local_addr, gni_mem_hdl->wc_post_desc.remote_addr);
    trios_start_timer(call_time);
    rc=GNI_PostRdma(gni_mem_hdl->wc_ep_hdl, &gni_mem_hdl->wc_post_desc);
    trios_stop_timer("PostRdma ack", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostRdma(rdma wc) failed: %d", rc);
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif


    log_debug(nnti_ee_debug_level, "exit");

    return;
}


static int need_mem_cq(const gni_memory_handle *gni_mem_hdl)
{
    int need_cq=0;

    assert(gni_mem_hdl);

    log_debug(nnti_ee_debug_level, "enter");

    switch (gni_mem_hdl->type) {
        case PUT_SRC_BUFFER:
        case GET_DST_BUFFER:
        case SEND_BUFFER:
            need_cq=0;
            break;
        case GET_SRC_BUFFER:
        case PUT_DST_BUFFER:
        case RESULT_BUFFER:
        case RECEIVE_BUFFER:
        case REQUEST_BUFFER:
            need_cq=1;
            break;
        case RDMA_TARGET_BUFFER:
            if ((gni_mem_hdl->last_op==GNI_OP_GET_INITIATOR) ||
                (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR)) {

                need_cq=0;
            } else {
                need_cq=1;
            }
            break;
        case UNKNOWN_BUFFER:
        default:
            need_cq=0;
            break;
    }

    log_debug(nnti_ee_debug_level, "exit");

    return(need_cq);
}

static gni_cq_handle_t get_cq(const NNTI_buffer_t *reg_buf)
{
    gni_memory_handle        *gni_mem_hdl=NULL;
    gni_connection           *conn=NULL;

    gni_cq_handle_t cq_hdl=0;
    gni_request_queue_handle *q_hdl=NULL;

    NNTI_result_t rc;

    assert(reg_buf);

    q_hdl      =&transport_global_data.req_queue;
    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    assert(gni_mem_hdl);

    log_debug(nnti_ee_debug_level, "enter");

    switch (gni_mem_hdl->type) {
        case PUT_SRC_BUFFER:
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_debug(nnti_cq_debug_level, "rdma initiator.  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
                cq_hdl=gni_mem_hdl->ep_cq_hdl;
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_debug(nnti_cq_debug_level, "rdma initiator.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                cq_hdl=gni_mem_hdl->wc_cq_hdl;
            }
            break;
        case GET_DST_BUFFER:
            if (gni_mem_hdl->op_state==RDMA_READ_INIT) {
                log_debug(nnti_cq_debug_level, "rdma initiator.  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
                cq_hdl=gni_mem_hdl->ep_cq_hdl;
            } else if (gni_mem_hdl->op_state==RDMA_READ_NEED_ACK) {
                log_debug(nnti_cq_debug_level, "rdma initiator.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                cq_hdl=gni_mem_hdl->wc_cq_hdl;
            }
            break;
        case SEND_BUFFER:
            if (gni_mem_hdl->last_op==GNI_OP_SEND) {
                log_debug(nnti_cq_debug_level, "send buffer using send op.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                cq_hdl=gni_mem_hdl->wc_cq_hdl;
            } else if (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR) {
                if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                    log_debug(nnti_cq_debug_level, "send buffer using put op.  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
                    cq_hdl=gni_mem_hdl->ep_cq_hdl;
                } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                    log_debug(nnti_cq_debug_level, "send buffer using put op.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                    cq_hdl=gni_mem_hdl->wc_cq_hdl;
                }
            }
            break;
        case GET_SRC_BUFFER:
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_READ_INIT) {
                log_debug(nnti_cq_debug_level, "rdma target (get_src).  waiting on mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->mem_cq_hdl);
                cq_hdl=gni_mem_hdl->mem_cq_hdl;
            } else if (gni_mem_hdl->op_state==RDMA_READ_NEED_ACK) {
                log_debug(nnti_cq_debug_level, "rdma target (get_src).  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
                cq_hdl=gni_mem_hdl->wc_mem_cq_hdl;
            }
#else
            log_debug(nnti_cq_debug_level, "rdma target (get_src).  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
            cq_hdl=gni_mem_hdl->wc_mem_cq_hdl;
#endif
            break;
        case PUT_DST_BUFFER:
        case RESULT_BUFFER:
        case RECEIVE_BUFFER:
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_debug(nnti_cq_debug_level, "rdma target (put_dest/result/receive).  waiting on mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->mem_cq_hdl);
                cq_hdl=gni_mem_hdl->mem_cq_hdl;
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_debug(nnti_cq_debug_level, "rdma target (put_dest/result/receive).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
                cq_hdl=gni_mem_hdl->wc_mem_cq_hdl;
            }
#else
            log_debug(nnti_cq_debug_level, "rdma target (put_dest/result/receive).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
            cq_hdl=gni_mem_hdl->wc_mem_cq_hdl;
#endif
            break;
        case RDMA_TARGET_BUFFER:
            if ((gni_mem_hdl->last_op==GNI_OP_GET_INITIATOR) ||
                (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR)) {

                if (gni_mem_hdl->op_state==RDMA_TARGET_INIT) {
                    log_debug(nnti_cq_debug_level, "rdma target (generic target).  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
                    cq_hdl=gni_mem_hdl->ep_cq_hdl;
                } else if (gni_mem_hdl->op_state==RDMA_TARGET_NEED_ACK) {
                    log_debug(nnti_cq_debug_level, "rdma target (generic target).  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                    cq_hdl=gni_mem_hdl->wc_cq_hdl;
                }
            } else {
#if defined(USE_RDMA_EVENTS)
                if (gni_mem_hdl->op_state==RDMA_TARGET_INIT) {
                    log_debug(nnti_cq_debug_level, "rdma target (generic target).  waiting on mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->mem_cq_hdl);
                    cq_hdl=gni_mem_hdl->mem_cq_hdl;
                } else if (gni_mem_hdl->op_state==RDMA_TARGET_NEED_ACK) {
                    log_debug(nnti_cq_debug_level, "rdma target (generic target).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
                    cq_hdl=gni_mem_hdl->wc_mem_cq_hdl;
                }
#else
                log_debug(nnti_cq_debug_level, "rdma target (generic target).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
                cq_hdl=gni_mem_hdl->wc_mem_cq_hdl;
#endif
            }
            break;
        case REQUEST_BUFFER:
            log_debug(nnti_event_debug_level, "request queue.  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)q_hdl->wc_mem_cq_hdl);
            cq_hdl=q_hdl->wc_mem_cq_hdl;
            break;
        case UNKNOWN_BUFFER:
        default:
            log_debug(nnti_debug_level, "unknown buffer type(%llu).", gni_mem_hdl->type);
            cq_hdl=(gni_cq_handle_t)-1;
            break;
    }

    log_debug(nnti_ee_debug_level, "exit");

    return(cq_hdl);
}

static void print_failed_cq(const NNTI_buffer_t *reg_buf)
{
    gni_memory_handle        *gni_mem_hdl=NULL;
    gni_request_queue_handle *q_hdl=NULL;

    assert(reg_buf);

    q_hdl      =&transport_global_data.req_queue;
    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    assert(gni_mem_hdl);

    switch (gni_mem_hdl->type) {
        case PUT_SRC_BUFFER:
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_error(nnti_debug_level, "rdma initiator.  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_error(nnti_debug_level, "rdma initiator.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
            }
            break;
        case GET_DST_BUFFER:
            if (gni_mem_hdl->op_state==RDMA_READ_INIT) {
                log_error(nnti_debug_level, "rdma initiator.  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
            } else if (gni_mem_hdl->op_state==RDMA_READ_NEED_ACK) {
                log_error(nnti_debug_level, "rdma initiator.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
            }
            break;
        case SEND_BUFFER:
            if (gni_mem_hdl->last_op==GNI_OP_SEND) {
                log_error(nnti_debug_level, "send buffer using send op.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
            } else if (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR) {
                if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                    log_error(nnti_debug_level, "send buffer using put op.  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
                } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                    log_error(nnti_debug_level, "send buffer using put op.  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                }
            }
            break;
        case GET_SRC_BUFFER:
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_READ_INIT) {
                log_error(nnti_debug_level, "rdma target (get_src).  waiting on mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->mem_cq_hdl);
            } else if (gni_mem_hdl->op_state==RDMA_READ_NEED_ACK) {
                log_error(nnti_debug_level, "rdma target (get_src).  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
            }
#else
            log_error(nnti_debug_level, "rdma target (get_src).  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
#endif
            break;
        case PUT_DST_BUFFER:
        case RESULT_BUFFER:
        case RECEIVE_BUFFER:
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_error(nnti_debug_level, "rdma target (put_dest/result/receive).  waiting on mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->mem_cq_hdl);
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_error(nnti_debug_level, "rdma target (put_dest/result/receive).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
            }
#else
            log_error(nnti_debug_level, "rdma target (put_dest/result/receive).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
#endif
            break;
        case RDMA_TARGET_BUFFER:
            if ((gni_mem_hdl->last_op==GNI_OP_GET_INITIATOR) ||
                (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR)) {

                if (gni_mem_hdl->op_state==RDMA_TARGET_INIT) {
                    log_error(nnti_debug_level, "rdma target (generic target).  waiting on ep_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->ep_cq_hdl);
                } else if (gni_mem_hdl->op_state==RDMA_TARGET_NEED_ACK) {
                    log_error(nnti_debug_level, "rdma target (generic target).  waiting on wc_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_cq_hdl);
                }
            } else {
#if defined(USE_RDMA_EVENTS)
                if (gni_mem_hdl->op_state==RDMA_TARGET_INIT) {
                    log_error(nnti_debug_level, "rdma target (generic target).  waiting on mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->mem_cq_hdl);
                } else if (gni_mem_hdl->op_state==RDMA_TARGET_NEED_ACK) {
                    log_error(nnti_debug_level, "rdma target (generic target).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
                }
#else
                log_error(nnti_debug_level, "rdma target (generic target).  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)gni_mem_hdl->wc_mem_cq_hdl);
#endif
            }
            break;
        case REQUEST_BUFFER:
            log_error(nnti_debug_level, "request queue.  waiting on wc_mem_cq_hdl(%llu).", (uint64_t)q_hdl->wc_mem_cq_hdl);
            break;
        case UNKNOWN_BUFFER:
        default:
            log_error(nnti_debug_level, "unknown buffer type(%llu).", gni_mem_hdl->type);
            break;
    }

    return;
}


static int process_event(
        const NNTI_buffer_t      *reg_buf,
        const NNTI_buf_ops_t      remote_op,
        gni_cq_handle_t           cq_hdl,
        gni_cq_entry_t           *ev_data,
        nnti_gni_work_completion *wc)
{
    int rc=NNTI_OK;
    gni_memory_handle *gni_mem_hdl=NULL;
    gni_connection *conn=NULL;

    void *buf=NULL;

//    gni_post_descriptor_t *post_desc_ptr;

    log_level debug_level=nnti_debug_level;

    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    log_debug(nnti_ee_debug_level, "enter");

    log_debug(nnti_debug_level, "reg_buf=%p; gni_mem_hdl->last_op=%d; remote_op=%d", reg_buf, gni_mem_hdl->last_op, remote_op);

    if (!GNI_CQ_STATUS_OK(*ev_data)) {
        return NNTI_EIO;
    }

    debug_level=nnti_debug_level;
    switch (gni_mem_hdl->type) {
        case SEND_BUFFER:
            if (gni_mem_hdl->last_op==GNI_OP_SEND) {
                log_debug(nnti_debug_level, "calling GetComplete(send req)");
                rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(fma send post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                print_post_desc(gni_mem_hdl->post_desc_ptr);

                memcpy(wc, &gni_mem_hdl->wc, sizeof(nnti_gni_work_completion));

                gni_mem_hdl->op_state=SEND_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.src_offset;
            } else if (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR) {
                if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                    log_debug(debug_level, "RDMA write event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);

                    log_debug(nnti_debug_level, "calling GetComplete(fma put send)");
                    rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(fma put send post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                    print_post_desc(gni_mem_hdl->post_desc_ptr);

                    log_debug(debug_level, "RDMA write (initiator) completion - reg_buf==%p", reg_buf);
                    gni_mem_hdl->op_state=RDMA_WRITE_NEED_ACK;
                    send_ack(reg_buf);
                } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                    log_debug(debug_level, "RDMA write ACK (initiator) completion - reg_buf==%p", reg_buf);

                    log_debug(nnti_debug_level, "calling GetComplete(fma put send ACK)");
                    rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(fma put send ACK post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                    print_post_desc(gni_mem_hdl->post_desc_ptr);

                    gni_mem_hdl->op_state = RDMA_WRITE_COMPLETE;
                    wc->byte_len   =gni_mem_hdl->wc.byte_len;
                    wc->byte_offset=gni_mem_hdl->wc.src_offset;
                }
            }
            break;
        case PUT_SRC_BUFFER:
            gni_mem_hdl->last_op=GNI_OP_PUT_INITIATOR;
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_debug(debug_level, "RDMA write event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);

                log_debug(nnti_debug_level, "calling GetComplete(rdma put src)");
                rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(put src post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                print_post_desc(gni_mem_hdl->post_desc_ptr);

                log_debug(debug_level, "RDMA write (initiator) completion - reg_buf==%p", reg_buf);
                gni_mem_hdl->op_state=RDMA_WRITE_NEED_ACK;
                send_ack(reg_buf);
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_debug(debug_level, "RDMA write ACK (initiator) completion - reg_buf==%p", reg_buf);

                log_debug(nnti_debug_level, "calling GetComplete(rdma put src ACK)");
                rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(put src ACK post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                print_post_desc(gni_mem_hdl->post_desc_ptr);

                gni_mem_hdl->op_state = RDMA_WRITE_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.src_offset;
            }
            break;
        case GET_DST_BUFFER:
            gni_mem_hdl->last_op=GNI_OP_GET_INITIATOR;
            if (gni_mem_hdl->op_state==RDMA_READ_INIT) {
                log_debug(debug_level, "RDMA read event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);

                log_debug(nnti_debug_level, "calling GetComplete(rdma get dst)");
                rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(get dst post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                print_post_desc(gni_mem_hdl->post_desc_ptr);

                log_debug(debug_level, "RDMA read (initiator) completion - reg_buf==%p", reg_buf);
                gni_mem_hdl->op_state=RDMA_READ_NEED_ACK;

                send_ack(reg_buf);

            } else if (gni_mem_hdl->op_state==RDMA_READ_NEED_ACK) {
                log_debug(debug_level, "RDMA read ACK (initiator) completion - reg_buf==%p", reg_buf);

                log_debug(nnti_debug_level, "calling GetComplete(rdma get dst ACK)");
                rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(get dst ACK post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                print_post_desc(gni_mem_hdl->post_desc_ptr);

                gni_mem_hdl->op_state = RDMA_READ_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.dest_offset;
            }
            break;
        case REQUEST_BUFFER:
            {
            uint64_t  index=0;
            gni_request_queue_handle *q=&transport_global_data.req_queue;

            gni_mem_hdl->last_op=GNI_OP_NEW_REQUEST;

            if (q->wc_buffer[q->req_processed].ack_received==1) {
                /* requests came out of order.  cleanup out of order requests here. */

                nnti_gni_work_completion *tmp_wc=&q->wc_buffer[q->req_processed];
                GNI_CQ_SET_INST_ID(*ev_data, tmp_wc->inst_id);
                conn = get_conn_instance(tmp_wc->inst_id);

                log_debug(debug_level, "recv completion - reg_buf=%p processing=%llu", reg_buf, q->req_processed);

                *wc=q->wc_buffer[q->req_processed];

                gni_mem_hdl->op_state = RECV_COMPLETE;

                q->wc_buffer[q->req_processed].ack_received=0;

                q->req_processed++;
                q->total_req_processed++;

            } else {

                log_debug(nnti_debug_level, "ev_data.data   ==%llu", (uint64_t)gni_cq_get_data(*ev_data));
                log_debug(nnti_debug_level, "ev_data.inst_id==%llu", (uint64_t)gni_cq_get_inst_id(*ev_data));

                index = (uint64_t)gni_cq_get_inst_id(*ev_data);
                log_debug(nnti_debug_level, "wc_index(%llu)", index);
                nnti_gni_work_completion *tmp_wc=&q->wc_buffer[index];
                tmp_wc->ack_received=1;
                GNI_CQ_SET_INST_ID(*ev_data, tmp_wc->inst_id);
                conn = get_conn_instance(tmp_wc->inst_id);

                if ((q->req_processed < q->req_count) &&
                        (q->wc_buffer[q->req_processed].ack_received==0)) {

                    log_debug(nnti_event_debug_level, "request received out of order (received index(%llu) ; received ack(%llu) ; waiting(%llu)) ; waiting ack(%llu)",
                            index, q->wc_buffer[index].ack_received, q->req_processed, q->wc_buffer[q->req_processed].ack_received);
                } else {
                    log_debug(debug_level, "recv completion - reg_buf=%p processing=%llu", reg_buf, q->req_processed);

                    *wc=q->wc_buffer[q->req_processed];

                    gni_mem_hdl->op_state = RECV_COMPLETE;

                    q->wc_buffer[q->req_processed].ack_received=0;

                    q->req_processed++;
                    q->total_req_processed++;
                }
            }

            if (q->req_processed > q->req_count) {
                log_error(nnti_debug_level, "req_processed(%llu) > req_count(%llu) fail",
                        q->req_processed, q->req_count);
            }
            if (q->req_processed == (q->req_count/2)) {
                if (q->req_index >= q->req_processed) {
                    reset_req_index(q);
                    send_unblock(q);
                    log_debug(nnti_event_debug_level, "resetting req_processed(%llu) total_req_processed(%llu)",
                            q->req_processed, q->total_req_processed);
                } else {
                    log_debug(nnti_event_debug_level, "skipping reset req_processed(%llu) total_req_processed(%llu)",
                            q->req_processed, q->total_req_processed);
                }
            }
            if (q->req_processed == q->req_processed_reset_limit) {
                q->req_processed=0;
            }
            log_debug(nnti_event_debug_level, "current req_processed(%llu) req_count(%llu)",
                    q->req_processed, q->req_count);
            log_debug(nnti_event_debug_level, "current received index(%llu) ; received ack(%llu) ; waiting(%llu)) ; waiting ack(%llu)",
                    index, q->wc_buffer[index].ack_received, q->req_processed, q->wc_buffer[q->req_processed].ack_received);
            }
            break;
        case RECEIVE_BUFFER:
            gni_mem_hdl->last_op=GNI_OP_RECEIVE;
            log_debug(debug_level, "receive buffer - recv completion - reg_buf==%p", reg_buf);

            gni_mem_hdl->op_state = RECV_COMPLETE;
            wc->byte_len   =gni_mem_hdl->wc.byte_len;
            wc->byte_offset=gni_mem_hdl->wc.dest_offset;
            break;
        case RESULT_BUFFER:
            gni_mem_hdl->last_op=GNI_OP_PUT_TARGET;
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_debug(debug_level, "RDMA write event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);
                log_debug(debug_level, "RDMA write (target) completion - reg_buf==%p", reg_buf);
                gni_mem_hdl->op_state=RDMA_WRITE_NEED_ACK;
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_debug(debug_level, "RDMA write ACK (target) completion - reg_buf==%p", reg_buf);

                gni_mem_hdl->op_state = RDMA_WRITE_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.dest_offset;
            }
#else
            log_debug(debug_level, "RDMA write ACK (target) completion - reg_buf==%p", reg_buf);

            gni_mem_hdl->op_state = RDMA_WRITE_COMPLETE;
            wc->byte_len   =gni_mem_hdl->wc.byte_len;
            wc->byte_offset=gni_mem_hdl->wc.dest_offset;
#endif
            break;
        case PUT_DST_BUFFER:
            gni_mem_hdl->last_op=GNI_OP_PUT_TARGET;
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_WRITE_INIT) {
                log_debug(debug_level, "RDMA write event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);
                log_debug(debug_level, "RDMA write (target) completion - reg_buf==%p", reg_buf);
                gni_mem_hdl->op_state=RDMA_WRITE_NEED_ACK;
            } else if (gni_mem_hdl->op_state==RDMA_WRITE_NEED_ACK) {
                log_debug(debug_level, "RDMA write ACK (target) completion - reg_buf==%p", reg_buf);

                gni_mem_hdl->op_state = RDMA_WRITE_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.dest_offset;
            }
#else
            log_debug(debug_level, "RDMA write ACK (target) completion - reg_buf==%p", reg_buf);

            gni_mem_hdl->op_state = RDMA_WRITE_COMPLETE;
            wc->byte_len   =gni_mem_hdl->wc.byte_len;
            wc->byte_offset=gni_mem_hdl->wc.dest_offset;
#endif
            break;
        case GET_SRC_BUFFER:
            gni_mem_hdl->last_op=GNI_OP_GET_TARGET;
#if defined(USE_RDMA_EVENTS)
            if (gni_mem_hdl->op_state==RDMA_READ_INIT) {
                log_debug(debug_level, "RDMA read event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);
                log_debug(debug_level, "RDMA read (tagret) completion - reg_buf==%p", reg_buf);
                gni_mem_hdl->op_state=RDMA_READ_NEED_ACK;
            } else if (gni_mem_hdl->op_state==RDMA_READ_NEED_ACK) {
                log_debug(debug_level, "RDMA read ACK (target) completion - reg_buf==%p", reg_buf);

                gni_mem_hdl->op_state = RDMA_READ_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.src_offset;
            }
#else
            log_debug(debug_level, "RDMA read ACK (target) completion - reg_buf==%p", reg_buf);

            gni_mem_hdl->op_state = RDMA_READ_COMPLETE;
            wc->byte_len   =gni_mem_hdl->wc.byte_len;
            wc->byte_offset=gni_mem_hdl->wc.src_offset;
#endif
            break;
        case RDMA_TARGET_BUFFER:
            log_debug(debug_level, "RDMA target completion - reg_buf=%p, op_state=%d, last_op=%d",
                    reg_buf, gni_mem_hdl->op_state, gni_mem_hdl->last_op);
            if ((gni_mem_hdl->last_op==GNI_OP_GET_INITIATOR) ||
                (gni_mem_hdl->last_op==GNI_OP_PUT_INITIATOR)) {

                if (gni_mem_hdl->op_state==RDMA_TARGET_INIT) {
                    log_debug(debug_level, "RDMA target event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);

                    log_debug(nnti_debug_level, "calling GetComplete(rdma target (mem cq)");
                    rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(post_desc_ptr) failed: %d", rc);
                    print_post_desc(gni_mem_hdl->post_desc_ptr);

                    gni_mem_hdl->op_state=RDMA_TARGET_NEED_ACK;

                    send_ack(reg_buf);

                } else if (gni_mem_hdl->op_state==RDMA_TARGET_NEED_ACK) {

                    log_debug(debug_level, "RDMA target ACK completion - reg_buf==%p", reg_buf);

                    log_debug(nnti_debug_level, "calling GetComplete(rdma target ACK)");
                    rc=GNI_GetCompleted (cq_hdl, *ev_data, &gni_mem_hdl->post_desc_ptr);
                    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted(rdma target ACK post_desc_ptr(%p)) failed: %d", gni_mem_hdl->post_desc_ptr, rc);
                    print_post_desc(gni_mem_hdl->post_desc_ptr);

                    gni_mem_hdl->op_state = RDMA_TARGET_COMPLETE;
                    wc->byte_len   =gni_mem_hdl->wc.byte_len;
                    wc->byte_offset=gni_mem_hdl->wc.dest_offset;

                }
            } else {
#if defined(USE_RDMA_EVENTS)
                if (gni_mem_hdl->op_state==RDMA_TARGET_INIT) {

                    log_debug(debug_level, "RDMA target event - reg_buf==%p, op_state==%d", reg_buf, gni_mem_hdl->op_state);
                    gni_mem_hdl->op_state=RDMA_TARGET_NEED_ACK;

                } else if (gni_mem_hdl->op_state==RDMA_TARGET_NEED_ACK) {

                    log_debug(debug_level, "RDMA target completion - reg_buf==%p", reg_buf);

                    gni_mem_hdl->op_state = RDMA_TARGET_COMPLETE;
                    wc->byte_len   =gni_mem_hdl->wc.byte_len;
                    wc->byte_offset=gni_mem_hdl->wc.dest_offset;

                }
#else
                log_debug(debug_level, "RDMA target completion - reg_buf==%p", reg_buf);

                gni_mem_hdl->op_state = RDMA_TARGET_COMPLETE;
                wc->byte_len   =gni_mem_hdl->wc.byte_len;
                wc->byte_offset=gni_mem_hdl->wc.dest_offset;
#endif
            }

            break;
    }

cleanup:
    log_debug(nnti_ee_debug_level, "exit");
    return (rc);
}

static int8_t is_buf_op_complete(
        const NNTI_buffer_t *reg_buf,
        const NNTI_buf_ops_t remote_op)
{
    int8_t rc=FALSE;
    gni_memory_handle *gni_mem_hdl=NULL;
    log_level nnti_debug_level = nnti_debug_level;

    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;

    log_debug(nnti_ee_debug_level, "enter");

    switch (gni_mem_hdl->type) {
        case SEND_BUFFER:
            if ((gni_mem_hdl->op_state == SEND_COMPLETE) ||
                (gni_mem_hdl->op_state == RDMA_WRITE_COMPLETE)) {
                rc=TRUE;
            }
            break;
        case PUT_SRC_BUFFER:
            if (gni_mem_hdl->op_state == RDMA_WRITE_COMPLETE) {
                rc=TRUE;
            }
            break;
        case GET_DST_BUFFER:
            if (gni_mem_hdl->op_state == RDMA_READ_COMPLETE) {
                rc=TRUE;
            }
            break;
        case REQUEST_BUFFER:
            if (gni_mem_hdl->op_state == RECV_COMPLETE) {
                rc=TRUE;
            }
            break;
        case RESULT_BUFFER:
        case RECEIVE_BUFFER:
            if (gni_mem_hdl->op_state == RDMA_WRITE_COMPLETE) {
                rc=TRUE;
            }
            break;
        case PUT_DST_BUFFER:
            if (gni_mem_hdl->op_state == RDMA_WRITE_COMPLETE) {
                rc=TRUE;
            }
            break;
        case GET_SRC_BUFFER:
            if (gni_mem_hdl->op_state == RDMA_READ_COMPLETE) {
                rc=TRUE;
            }
            break;
        case RDMA_TARGET_BUFFER:
            if (gni_mem_hdl->op_state == RDMA_TARGET_COMPLETE) {
                rc=TRUE;
            }
            break;
    }

    log_debug(nnti_ee_debug_level, "exit");
    return(rc);
}

static void create_peer(NNTI_peer_t *peer, char *name, NNTI_ip_addr addr, NNTI_tcp_port port, uint32_t ptag, uint32_t cookie, NNTI_instance_id instance)
{
    log_debug(nnti_ee_debug_level, "enter");

    sprintf(peer->url, "gni://%s:%u/?ptag=%llu&cookie=%llu", name, ntohs(port), (uint64_t)ptag, (uint64_t)cookie);

    peer->peer.transport_id                       =NNTI_TRANSPORT_GEMINI;
    peer->peer.NNTI_remote_process_t_u.gni.addr   =addr;
    peer->peer.NNTI_remote_process_t_u.gni.port   =port;
    peer->peer.NNTI_remote_process_t_u.gni.inst_id=instance;

    log_debug(nnti_ee_debug_level, "exit");
}

static void copy_peer(NNTI_peer_t *src, NNTI_peer_t *dest)
{
    log_debug(nnti_ee_debug_level, "enter");

    strncpy(dest->url, src->url, NNTI_URL_LEN);

    src->peer.transport_id                        =NNTI_TRANSPORT_GEMINI;
    dest->peer.NNTI_remote_process_t_u.gni.addr   =src->peer.NNTI_remote_process_t_u.gni.addr;
    dest->peer.NNTI_remote_process_t_u.gni.port   =src->peer.NNTI_remote_process_t_u.gni.port;
    dest->peer.NNTI_remote_process_t_u.gni.inst_id=src->peer.NNTI_remote_process_t_u.gni.inst_id;

    log_debug(nnti_ee_debug_level, "exit");
}

static void write_contact_info(void)
{
    trios_declare_timer(call_time);
    char *contact_filename=NULL;
    FILE *cf=NULL;

    trios_start_timer(call_time);
    contact_filename=getenv("NNTI_CONTACT_FILENAME");
    trios_stop_timer("getenv", call_time);
    trios_start_timer(call_time);
    cf=fopen(contact_filename, "w");
    trios_stop_timer("fopen", call_time);
    trios_start_timer(call_time);
    fprintf(cf, "gni://%s:%u/", transport_global_data.listen_name, ntohs(transport_global_data.listen_port));
    trios_stop_timer("fprintf", call_time);
    trios_start_timer(call_time);
    fclose(cf);
    trios_stop_timer("fclose", call_time);
}

static int init_server_listen_socket()
{
    NNTI_result_t rc=NNTI_OK;
    trios_declare_timer(call_time);
    int flags;
    struct hostent *host_entry;
    struct sockaddr_in skin;
    socklen_t skin_size=sizeof(struct sockaddr_in);

    trios_start_timer(call_time);
    transport_global_data.listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    trios_stop_timer("socket", call_time);
    if (transport_global_data.listen_sock < 0)
        log_error(nnti_debug_level, "failed to create tcp socket: %s", strerror(errno));
    flags = 1;
    trios_start_timer(call_time);
    if (setsockopt(transport_global_data.listen_sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) < 0)
        log_error(nnti_debug_level, "failed to set tcp socket REUSEADDR flag: %s", strerror(errno));
    trios_stop_timer("setsockopt", call_time);

    log_debug(nnti_debug_level, "listen_name (%s).", transport_global_data.listen_name);
    if (transport_global_data.listen_name[0]!='\0') {
        log_debug(nnti_debug_level, "using hostname from command-line (%s).", transport_global_data.listen_name);
    } else {
        trios_start_timer(call_time);
        gethostname(transport_global_data.listen_name, NNTI_HOSTNAME_LEN);
        trios_stop_timer("gethostname", call_time);
        log_debug(nnti_debug_level, "hostname not given on command-line.  using gethostname() result (%s).", transport_global_data.listen_name);
    }

    /* lookup the host provided on the command line */
    trios_start_timer(call_time);
    host_entry = gethostbyname(transport_global_data.listen_name);
    trios_stop_timer("gethostbyname", call_time);
    if (!host_entry) {
        log_warn(nnti_debug_level, "failed to resolve server name (%s): %s", transport_global_data.listen_name, strerror(errno));
        return NNTI_ENOENT;
    }

    memset(&skin, 0, sizeof(skin));
    skin.sin_family = AF_INET;
    memcpy(&skin.sin_addr, host_entry->h_addr_list[0], (size_t) host_entry->h_length);
    /* 0 here means to bind to a random port assigned by the kernel */
    skin.sin_port = 0;

retry:
    trios_start_timer(call_time);
    if (bind(transport_global_data.listen_sock, (struct sockaddr *)&skin, skin_size) < 0) {
        if (errno == EINTR) {
            goto retry;
        } else {
            log_error(nnti_debug_level, "failed to bind tcp socket: %s", strerror(errno));
        }
    }
    trios_stop_timer("bind", call_time);


    /* after the bind, get the "name" for the socket.  the "name" contains the port assigned by the kernel. */
    trios_start_timer(call_time);
    getsockname(transport_global_data.listen_sock, (struct sockaddr *)&skin, &skin_size);
    trios_stop_timer("getsockname", call_time);
    transport_global_data.listen_addr = (uint32_t)skin.sin_addr.s_addr;
    transport_global_data.listen_port = (uint16_t)skin.sin_port;
    log_debug(nnti_debug_level, "listening on ip(%s) addr(%u) port(%u)",
            transport_global_data.listen_name,
            (unsigned int)ntohl(skin.sin_addr.s_addr),
            (unsigned int)ntohs(skin.sin_port));
    trios_start_timer(call_time);
    if (listen(transport_global_data.listen_sock, 1024) < 0)
        log_error(nnti_debug_level, "failed to listen on tcp socket: %s", strerror(errno));
    trios_stop_timer("listen", call_time);

    return rc;
}

static void transition_connection_to_ready(
        int sock,
        gni_connection *conn)
{
    int i;
    int rc=NNTI_OK;
    trios_declare_timer(callTime);

    trios_start_timer(callTime);
    /* final sychronization to ensure both sides have posted RTRs */
    rc = tcp_exchange(sock, 0, &rc, &rc, sizeof(rc));
    trios_stop_timer("transition tcp_exchange", callTime);
}

/*
 * Try hard to read the whole buffer.  Abort on read error.
 */
static int tcp_read(int sock, void *incoming, size_t len)
{
    int bytes_this_read=0;
    int bytes_left=len;
    int bytes_read=0;

    while (bytes_left > 0) {
        bytes_this_read = read(sock, (char *)incoming + bytes_read, bytes_left);
        if (bytes_this_read < 0) {
            return bytes_this_read;
        }
        if (bytes_this_read == 0) {
            break;
        }
        bytes_left -= bytes_this_read;
        bytes_read += bytes_this_read;
    }
    return bytes_read;
}

/*
 * Try hard to write the whole buffer.  Abort on write error.
 */
static int tcp_write(int sock, const void *outgoing, size_t len)
{
    int bytes_this_write=0;
    int bytes_left=len;
    int bytes_written=0;

    while (bytes_left > 0) {
        bytes_this_write = write(sock, (const char *)outgoing + bytes_written, bytes_left);
        if (bytes_this_write < 0) {
            return bytes_this_write;
        }
        bytes_left    -= bytes_this_write;
        bytes_written += bytes_this_write;
    }
    return bytes_written;
}

/*
 * Two processes exchange data over a TCP socket.  Both sides send and receive the
 * same amount of data.  Only one process can declare itself the server (is_server!=0),
 * otherwise this will hang because both will wait for the read to complete.
 *
 * Server receives, then sends.
 * Client sends, then receives.
 */
static int tcp_exchange(int sock, int is_server, void *incoming, void *outgoing, size_t len)
{
    int rc=0;

    if (is_server) {
        trios_declare_timer(callTime);
        trios_start_timer(callTime);
        rc = tcp_read(sock, incoming, len);
        trios_stop_timer("tcp_read", callTime);
        if (rc < 0) {
            log_warn(nnti_debug_level, "server failed to read GNI connection info: errno=%d", errno);
            goto out;
        }
        if (rc != (int) len) {
            log_error(nnti_debug_level, "partial read, %d/%d bytes", rc, (int) len);
            rc = 1;
            goto out;
        }
    } else {
        trios_declare_timer(callTime);
        trios_start_timer(callTime);
        rc = tcp_write(sock, outgoing, len);
        trios_stop_timer("tcp_write", callTime);
        if (rc < 0) {
            log_warn(nnti_debug_level, "client failed to write GNI connection info: errno=%d", errno);
            goto out;
        }
    }

    if (is_server) {
        trios_declare_timer(callTime);
        trios_start_timer(callTime);
        rc = tcp_write(sock, outgoing, len);
        trios_stop_timer("tcp_write", callTime);
        if (rc < 0) {
            log_warn(nnti_debug_level, "server failed to write GNI connection info: errno=%d", errno);
            goto out;
        }
    } else {
        trios_declare_timer(callTime);
        trios_start_timer(callTime);
        rc = tcp_read(sock, incoming, len);
        trios_stop_timer("tcp_read", callTime);
        if (rc < 0) {
            log_warn(nnti_debug_level, "client failed to read GNI connection info: errno=%d", errno);
            goto out;
        }
        if (rc != (int) len) {
            log_error(nnti_debug_level, "partial read, %d/%d bytes", rc, (int) len);
            rc = 1;
            goto out;
        }
    }

    rc = 0;

out:
    return rc;
}

static int new_client_connection(
        gni_connection *c,
        int sock)
{
    int i, j, rc;
    int num_wr;
    size_t len;

    /*
     * Values passed through TCP to permit Gemini connection.
     */
    struct {
        NNTI_instance_id instance;
        alpsAppGni_t     alps_info;
    } instance_in, instance_out;
    struct {
        nnti_gni_server_queue_attrs server_attrs;
    } sa_in;
    struct {
        nnti_gni_client_queue_attrs client_attrs;
    } ca_out;

    trios_declare_timer(call_time);

    c->connection_type=CLIENT_CONNECTION;

    instance_out.instance  = htonl(transport_global_data.instance);
    instance_out.alps_info = transport_global_data.alps_info;

    trios_start_timer(call_time);
    rc = tcp_exchange(sock, 0, &instance_in, &instance_out, sizeof(instance_in));
    trios_stop_timer("tcp_exchange", call_time);
    if (rc)
        goto out;

    c->peer_instance = ntohl(instance_in.instance);
    c->peer_instance  = ntohl(instance_in.instance);
    c->peer_alps_info = instance_in.alps_info;


    memset(&sa_in, 0, sizeof(sa_in));

    trios_start_timer(call_time);
    rc = tcp_read(sock, &sa_in, sizeof(sa_in));
    trios_stop_timer("read server queue attrs", call_time);
    if (rc == sizeof(sa_in)) {
        rc=0;
    }
    if (rc)
        goto out;

    c->queue_remote_attrs.server=sa_in.server_attrs;

    client_req_queue_init(c);

    memset(&ca_out, 0, sizeof(ca_out));

    ca_out.client_attrs.unblock_buffer_addr=c->queue_local_attrs.unblock_buffer_addr;
    ca_out.client_attrs.unblock_mem_hdl    =c->queue_local_attrs.unblock_mem_hdl;

    trios_start_timer(call_time);
    rc = tcp_write(sock, &ca_out, sizeof(ca_out));
    trios_stop_timer("write client queue attrs", call_time);
    if (rc == sizeof(ca_out)) {
        rc=0;
    }
    if (rc)
        goto out;

out:
    return rc;
}

static int new_server_connection(
        gni_connection *c,
        int sock)
{
    int i, j, rc;
    int num_wr;
    size_t len;

    /*
     * Values passed through TCP to permit Gemini connection.
     */
    struct {
        NNTI_instance_id instance;
        alpsAppGni_t     alps_info;
    } instance_in, instance_out;
    struct {
        nnti_gni_server_queue_attrs server_attrs;
    } sa_out;
    struct {
        nnti_gni_client_queue_attrs client_attrs;
    } ca_in;

    trios_declare_timer(call_time);

    assert(transport_global_data.req_queue.reg_buf);

    gni_memory_handle *gni_mem_hdl=(gni_memory_handle *)transport_global_data.req_queue.reg_buf->transport_private;


    c->connection_type=SERVER_CONNECTION;

    instance_out.instance  = htonl(transport_global_data.instance);
    instance_out.alps_info = transport_global_data.alps_info;

    trios_start_timer(call_time);
    rc = tcp_exchange(sock, 1, &instance_in, &instance_out, sizeof(instance_in));
    trios_stop_timer("tcp_exchange", call_time);
    if (rc)
        goto out;

    c->peer_instance  = ntohl(instance_in.instance);
    c->peer_alps_info = instance_in.alps_info;
    c->peer_ptag      = instance_in.alps_info.ptag;
    c->peer_cookie    = instance_in.alps_info.cookie;
    c->cdm_hdl = transport_global_data.cdm_hdl;
    c->nic_hdl = transport_global_data.nic_hdl;

    memset(&sa_out, 0, sizeof(sa_out));

    sa_out.server_attrs.req_index_addr   =transport_global_data.req_queue.req_index_addr;
    sa_out.server_attrs.req_index_mem_hdl=transport_global_data.req_queue.req_index_mem_hdl;
    sa_out.server_attrs.req_buffer_addr  =(uint64_t)transport_global_data.req_queue.req_buffer;
    sa_out.server_attrs.req_size         =transport_global_data.req_queue.req_size;
    sa_out.server_attrs.req_count        =transport_global_data.req_queue.req_count;
    sa_out.server_attrs.req_mem_hdl      =gni_mem_hdl->mem_hdl;
    sa_out.server_attrs.wc_buffer_addr   =(uint64_t)transport_global_data.req_queue.wc_buffer;
    sa_out.server_attrs.wc_mem_hdl       =transport_global_data.req_queue.wc_mem_hdl;

    trios_start_timer(call_time);
    rc = tcp_write(sock, &sa_out, sizeof(sa_out));
    trios_stop_timer("write server queue attrs", call_time);
    if (rc == sizeof(sa_out)) {
        rc=0;
    }
    if (rc)
        goto out;

    memset(&ca_in, 0, sizeof(ca_in));

    trios_start_timer(call_time);
    rc = tcp_read(sock, &ca_in, sizeof(ca_in));
    trios_stop_timer("read client queue attrs", call_time);
    if (rc == sizeof(ca_in)) {
        rc=0;
    }
    if (rc)
        goto out;

    c->queue_remote_attrs.client=ca_in.client_attrs;

out:
    return rc;
}

static NNTI_result_t insert_conn_peer(const NNTI_peer_t *peer, gni_connection *conn)
{
    NNTI_result_t  rc=NNTI_OK;
    addrport_key key;

    key.addr = peer->peer.NNTI_remote_process_t_u.gni.addr;
    key.port = peer->peer.NNTI_remote_process_t_u.gni.port;

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_peer(logger_get_file(), "peer",
                "insert_conn_peer", peer);
    }

    nthread_lock(&nnti_conn_peer_lock);
    connections_by_peer[key] = conn;   // add to connection map
    nthread_unlock(&nnti_conn_peer_lock);

    log_debug(nnti_debug_level, "peer connection added (conn=%p)", conn);

    return(rc);
}
static NNTI_result_t insert_conn_instance(const NNTI_instance_id instance, gni_connection *conn)
{
    NNTI_result_t  rc=NNTI_OK;

    nthread_lock(&nnti_conn_instance_lock);
    assert(connections_by_instance.find(instance) == connections_by_instance.end());
    connections_by_instance[instance] = conn;
    nthread_unlock(&nnti_conn_instance_lock);

    log_debug(nnti_debug_level, "instance connection added (conn=%p)", conn);

    return(rc);
}
static void print_peer_map()
{
    conn_by_peer_iter_t i;

    for (i=connections_by_peer.begin(); i != connections_by_peer.end(); i++) {
        log_debug(nnti_debug_level, "peer_map key=(%llu,%llu) conn=%p",
                (uint64_t)i->first.addr, (uint64_t)i->first.port, i->second);
    }
}


static gni_connection *get_conn_peer(const NNTI_peer_t *peer)
{
    NNTI_result_t  rc=NNTI_OK;
    gni_connection *conn = NULL;

    addrport_key   key;

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_peer(logger_get_file(), "peer",
                "get_conn_peer", peer);
    }

    memset(&key, 0, sizeof(addrport_key));
    key.addr=peer->peer.NNTI_remote_process_t_u.gni.addr;
    key.port=peer->peer.NNTI_remote_process_t_u.gni.port;

    nthread_lock(&nnti_conn_peer_lock);
    conn = connections_by_peer[key];
    nthread_unlock(&nnti_conn_peer_lock);

    if (conn != NULL) {
        log_debug(nnti_debug_level, "connection found");
        return conn;
    }

    log_debug(nnti_debug_level, "connection NOT found");
    print_peer_map();

    return(NULL);
}
static void print_instance_map()
{
    NNTI_result_t   rc=NNTI_OK;

    conn_by_inst_iter_t i;
    for (i=connections_by_instance.begin(); i != connections_by_instance.end(); i++) {
        log_debug(nnti_debug_level, "instance_map key=%llu conn=%p", i->first, i->second);
    }
}
static gni_connection *get_conn_instance(const NNTI_instance_id instance)
{
    NNTI_result_t  rc=NNTI_OK;
    gni_connection *conn=NULL;

    log_debug(nnti_debug_level, "looking for instance=%llu", (unsigned long long)instance);
    nthread_lock(&nnti_conn_instance_lock);
    conn = connections_by_instance[instance];
    nthread_unlock(&nnti_conn_instance_lock);

    if (conn != NULL) {
        log_debug(nnti_debug_level, "connection found");
        return conn;
    }

    log_debug(nnti_debug_level, "connection NOT found");
    print_instance_map();

    return(NULL);
}
static gni_connection *del_conn_peer(const NNTI_peer_t *peer)
{
    NNTI_result_t   rc=NNTI_OK;
    gni_connection *conn=NULL;
    addrport_key    key;

    if (logging_debug(nnti_debug_level)) {
        fprint_NNTI_peer(logger_get_file(), "peer",
                "get_conn_peer", peer);
    }

    memset(&key, 0, sizeof(addrport_key));
    key.addr=peer->peer.NNTI_remote_process_t_u.gni.addr;
    key.port=peer->peer.NNTI_remote_process_t_u.gni.port;

    nthread_lock(&nnti_conn_peer_lock);
    conn = connections_by_peer[key];
    nthread_unlock(&nnti_conn_peer_lock);

    if (conn != NULL) {
        log_debug(nnti_debug_level, "connection found");
        connections_by_peer.erase(key);
        del_conn_instance(conn->peer_instance);
    } else {
        log_debug(nnti_debug_level, "connection NOT found");
    }

    return(conn);
}
static gni_connection *del_conn_instance(const NNTI_instance_id instance)
{
    NNTI_result_t   rc=NNTI_OK;
    gni_connection *conn=NULL;
    log_level debug_level = nnti_debug_level;

    nthread_lock(&nnti_conn_instance_lock);
    conn = connections_by_instance[instance];
    nthread_unlock(&nnti_conn_instance_lock);

    if (conn != NULL) {
        log_debug(debug_level, "connection found");
        connections_by_instance.erase(instance);
    } else {
        log_debug(debug_level, "connection NOT found");
    }

    return(conn);
}
static void close_all_conn(void)
{
    log_level debug_level = nnti_debug_level;

    log_debug(debug_level, "enter (%d instance connections, %d peer connections)",
            connections_by_instance.size(), connections_by_peer.size());

    nthread_lock(&nnti_conn_instance_lock);
    conn_by_inst_iter_t inst_iter;
    for (inst_iter = connections_by_instance.begin(); inst_iter != connections_by_instance.end(); inst_iter++) {
        log_debug(debug_level, "close connection (instance=%llu)", inst_iter->first);
        close_connection(inst_iter->second);
        connections_by_instance.erase(inst_iter);
    }
#if 0
    HASH_ITER(hh, connections_by_instance, current_inst, tmp_inst) {
        HASH_DEL(connections_by_instance,current_inst);
        close_connection(current_inst->conn);
    }
#endif

    nthread_unlock(&nnti_conn_instance_lock);

    nthread_lock(&nnti_conn_peer_lock);
    conn_by_peer_iter_t peer_iter;
    for (peer_iter = connections_by_peer.begin(); peer_iter != connections_by_peer.end(); peer_iter++) {
        log_debug(debug_level, "close connection (peer.addr=%llu)", peer_iter->first.addr);
        close_connection(peer_iter->second);
        connections_by_peer.erase(peer_iter);
    }

#if 0
    HASH_ITER(hh, connections_by_peer, current_peer, tmp_peer) {
        HASH_DEL(connections_by_peer,current_peer);
        if (current_peer->conn->state!=DISCONNECTED) {
            close_connection(current_peer->conn);
        }
    }
#endif
    nthread_unlock(&nnti_conn_peer_lock);

    log_debug(debug_level, "exit (%d instance connections, %d peer connections)",
            connections_by_instance.size(), connections_by_peer.size());

    return;
}

/**
 * @brief initialize
 */
static NNTI_result_t init_connection(
        gni_connection **conn,
        const int sock,
        const char *peername,
        const NNTI_ip_addr  addr,
        const NNTI_tcp_port port,
        const int is_server)
{
    int rc=NNTI_OK; /* return code */
    struct ibv_recv_wr *bad_wr;

    trios_declare_timer(call_time);

    log_debug(nnti_debug_level, "initializing gni connection");

    (*conn)->peer_name = strdup(peername);
    (*conn)->peer_addr = addr;
    (*conn)->peer_port = htons(port);

    trios_start_timer(call_time);
    if (is_server) {
        rc = new_server_connection(*conn, sock);
    } else {
        rc = new_client_connection(*conn, sock);
    }
    trios_stop_timer("new connection", call_time);
    if (rc) {
        close_connection(*conn);
        goto out;
    }

    print_gni_conn(*conn);

out:
    return((NNTI_result_t)rc);
}

/*
 * At an explicit BYE message, or at finalize time, shut down a connection.
 * If descriptors are posted, defer and clean up the connection structures
 * later.
 */
static void close_connection(gni_connection *c)
{
    int rc;
    int i;
    log_level debug_level = nnti_debug_level;  // nnti_ee_debug_level;

    if (c==NULL) return;

    log_debug(debug_level, "close_connection: start");

    print_gni_conn(c);

    if (c->peer_name) {
        free(c->peer_name);
        c->peer_name = NULL;
    }

    if (c->connection_type == CLIENT_CONNECTION) {
        client_req_queue_destroy(c);
    }

    c->state=DISCONNECTED;

    log_debug(debug_level, "close_connection: exit");
}

/**
 * Check for new connections.  The listening socket is left nonblocking
 * so this test can be quick; but accept is not really that quick compared
 * to polling an Gemini interface, for instance.  Returns >0 if an accept worked.
 */
static int check_listen_socket_for_new_connections()
{
    NNTI_result_t rc = NNTI_OK;

    struct sockaddr_in ssin;
    socklen_t len;
    int s;
    gni_connection *conn = NULL;
    NNTI_peer_t *peer=NULL;

    len = sizeof(ssin);
    s = accept(transport_global_data.listen_sock, (struct sockaddr *) &ssin, &len);
    if (s < 0) {
        if (!(errno == EAGAIN)) {
            log_error(nnti_debug_level, "failed to accept tcp socket connection: %s", strerror(errno));
            rc = NNTI_EIO;
        }
    } else {
        char         *peer_hostname = strdup(inet_ntoa(ssin.sin_addr));
        NNTI_ip_addr  peer_addr  = ssin.sin_addr.s_addr;
        NNTI_tcp_port peer_port  = ntohs(ssin.sin_port);

        peer=(NNTI_peer_t *)malloc(sizeof(NNTI_peer_t));
        log_debug(nnti_debug_level, "malloc returned peer=%p.", peer);
        if (peer == NULL) {
            log_error(nnti_debug_level, "malloc returned NULL.  out of memory?: %s", strerror(errno));
            rc=NNTI_ENOMEM;
            goto cleanup;
        }

        conn = (gni_connection *)calloc(1, sizeof(gni_connection));
        log_debug(nnti_debug_level, "calloc returned conn=%p.", conn);
        if (conn == NULL) {
            log_error(nnti_debug_level, "calloc returned NULL.  out of memory?: %s", strerror(errno));
            rc=NNTI_ENOMEM;
            goto cleanup;
        }

//        nthread_lock(&nnti_gni_lock);
        rc=init_connection(&conn, s, peer_hostname, peer_addr, peer_port, 1);
        if (rc!=NNTI_OK) {
            goto cleanup;
        }
        create_peer(
                peer,
                peer_hostname,
                ssin.sin_addr.s_addr,
                ssin.sin_port,
                conn->peer_ptag,
                conn->peer_cookie,
                conn->peer_instance);
        insert_conn_instance(conn->peer_instance, conn);
        insert_conn_peer(peer, conn);

        transition_connection_to_ready(s, conn);
//        nthread_unlock(&nnti_gni_lock);

        log_debug(nnti_debug_level, "accepted new connection from %s:%u", peer_hostname, peer_port);

        if (close(s) < 0) {
            log_error(nnti_debug_level, "failed to close new tcp socket");
        }

        if (logging_debug(nnti_debug_level)) {
            fprint_NNTI_peer(logger_get_file(), "peer",
                    "end of check_listen_socket_for_new_connections", peer);
        }

        nthread_yield();
    }

cleanup:
    if (rc != NNTI_OK) {
        if (peer!=NULL) free(peer);
        if (conn!=NULL) free(conn);
    }
    return rc;
}

/**
 * @brief Continually check for new connection attempts.
 *
 */
static void *connection_listener_thread(void *args)
{
    int rc=NNTI_OK;

    log_debug(nnti_debug_level, "started thread to listen for client connection attempts");

    /* SIGINT (Ctrl-C) will get us out of this loop */
    while (!trios_exit_now()) {
        log_debug(nnti_debug_level, "listening for new connection");
        rc = check_listen_socket_for_new_connections();
        if (rc != NNTI_OK) {
            log_fatal(nnti_debug_level, "error returned from trios_gni_server_listen_for_client: %d", rc);
            continue;
        }
    }

    nthread_exit(&rc);

    return(NULL);
}

/**
 * @brief Start a thread to check for new connection attempts.
 *
 */
static int start_connection_listener_thread()
{
    int rc = NNTI_OK;
    nthread_t thread;

    /* Create the thread. Do we want special attributes for this? */
    rc = nthread_create(&thread, NULL, connection_listener_thread, NULL);
    if (rc) {
        log_error(nnti_debug_level, "could not spawn thread");
        rc = NNTI_EBADRPC;
    }

    return rc;
}


/* Borrowed from util-linux-2.13-pre7/schedutils/taskset.c */
static char *cpuset_to_cstr(cpu_set_t *mask, char *str)
{
  char *ptr = str;
  int i, j, entry_made = 0;
  for (i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, mask)) {
      int run = 0;
      entry_made = 1;
      for (j = i + 1; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, mask)) run++;
        else break;
      }
      if (!run)
        sprintf(ptr, "%d,", i);
      else if (run == 1) {
        sprintf(ptr, "%d,%d,", i, i + 1);
        i++;
      } else {
        sprintf(ptr, "%d-%d,", i, i + run);
        i += run;
      }
      while (*ptr != 0) ptr++;
    }
  }
  ptr -= entry_made;
  *ptr = 0;
  return(str);
}

static uint32_t get_cpunum(void)
{
  int i, j, entry_made = 0;
  uint32_t cpu_num;

  cpu_set_t coremask;

  (void)sched_getaffinity(0, sizeof(coremask), &coremask);

  for (i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &coremask)) {
      int run = 0;
      for (j = i + 1; j < CPU_SETSIZE; j++) {
        if (CPU_ISSET(j, &coremask)) run++;
        else break;
      }
      if (!run) {
        cpu_num=i;
      } else {
        fprintf(stdout, "This thread is bound to multiple CPUs(%d).  Using lowest numbered CPU(%d).", run+1, cpu_num);
        cpu_num=i;
      }
    }
  }
  return(cpu_num);
}


static void get_alps_info(
        alpsAppGni_t *alps_info)
{
    int alps_rc=0;
    int req_rc=0;
    size_t rep_size=0;

    uint64_t apid=0;
    alpsAppLLIGni_t *alps_info_list;
    char buf[1024];

    alps_info_list=(alpsAppLLIGni_t *)&buf[0];

    alps_app_lli_lock();

    log_debug(nnti_debug_level, "sending ALPS request");
    alps_rc = alps_app_lli_put_request(ALPS_APP_LLI_ALPS_REQ_GNI, NULL, 0);
    if (alps_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_put_request failed: %d", alps_rc);
    log_debug(nnti_debug_level, "waiting for ALPS reply");
    alps_rc = alps_app_lli_get_response(&req_rc, &rep_size);
    if (alps_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_get_response failed: alps_rc=%d", alps_rc);
    if (req_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_get_response failed: req_rc=%d", req_rc);
    if (rep_size != 0) {
        log_debug(nnti_debug_level, "waiting for ALPS reply bytes (%d) ; sizeof(alps_info)==%d ; sizeof(alps_info_list)==%d", rep_size, sizeof(alps_info), sizeof(alps_info_list));
        alps_rc = alps_app_lli_get_response_bytes(alps_info_list, rep_size);
        if (alps_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_get_response_bytes failed: %d", alps_rc);
    }

    log_debug(nnti_debug_level, "sending ALPS request");
    alps_rc = alps_app_lli_put_request(ALPS_APP_LLI_ALPS_REQ_APID, NULL, 0);
    if (alps_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_put_request failed: %d", alps_rc);
    log_debug(nnti_debug_level, "waiting for ALPS reply");
    alps_rc = alps_app_lli_get_response(&req_rc, &rep_size);
    if (alps_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_get_response failed: alps_rc=%d", alps_rc);
    if (req_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_get_response failed: req_rc=%d", req_rc);
    if (rep_size != 0) {
        log_debug(nnti_debug_level, "waiting for ALPS reply bytes (%d) ; sizeof(apid)==%d", rep_size, sizeof(apid));
        alps_rc = alps_app_lli_get_response_bytes(&apid, rep_size);
        if (alps_rc != 0) log_debug(nnti_debug_level, "alps_app_lli_get_response_bytes failed: %d", alps_rc);
    }

    alps_app_lli_unlock();

    memcpy(alps_info, (alpsAppGni_t*)alps_info_list->u.buf, sizeof(alpsAppGni_t));
    transport_global_data.apid=apid;

    log_debug(nnti_debug_level, "apid                 =%llu", (unsigned long long)apid);
    log_debug(nnti_debug_level, "alps_info->device_id =%llu", (unsigned long long)alps_info->device_id);
    log_debug(nnti_debug_level, "alps_info->local_addr=%lld", (long long)alps_info->local_addr);
    log_debug(nnti_debug_level, "alps_info->cookie    =%llu", (unsigned long long)alps_info->cookie);
    log_debug(nnti_debug_level, "alps_info->ptag      =%llu", (unsigned long long)alps_info->ptag);

    log_debug(nnti_debug_level, "ALPS response - apid(%llu) alps_info->device_id(%llu) alps_info->local_addr(%llu) "
            "alps_info->cookie(%llu) alps_info->ptag(%llu)",
            (unsigned long long)apid,
            (unsigned long long)alps_info->device_id,
            (long long)alps_info->local_addr,
            (unsigned long long)alps_info->cookie,
            (unsigned long long)alps_info->ptag);

    return;
}

static void print_wc(const nnti_gni_work_completion *wc)
{
    log_debug(nnti_debug_level, "wc=%p, wc.op=%d, wc.inst_id=%llu, wc.byte_len=%llu, wc.byte_offset=%llu, wc.src_offset=%llu, wc.dest_offset=%llu",
            wc,
            wc->op,
            wc->inst_id,
            wc->byte_len,
            wc->byte_offset,
            wc->src_offset,
            wc->dest_offset);
}

static void print_cq_event(
        const gni_cq_entry_t *event)
{
    if (gni_cq_get_status(*event) != 0) {
        log_error(nnti_debug_level, "event=%p, event.data=%d, event.source=%d, event.status=%lu, "
                "event.info=%u, event.overrun=%u, event.inst_id=%u, event.tid=%u, event.msg_id=%u, event.type=%u",
                event,
                (uint64_t)gni_cq_get_data(*event),
                (uint64_t)gni_cq_get_source(*event),
                (uint64_t)gni_cq_get_status(*event),
                (uint64_t)gni_cq_get_info(*event),
                (uint64_t)gni_cq_overrun(*event),
                (uint64_t)gni_cq_get_inst_id(*event),
                (uint64_t)gni_cq_get_tid(*event),
                (uint64_t)gni_cq_get_msg_id(*event),
                (uint64_t)gni_cq_get_type(*event));
    } else {
        log_debug(nnti_debug_level, "event=%p, event.data=%d, event.source=%d, event.status=%lu, "
                "event.info=%u, event.overrun=%u, event.inst_id=%u, event.tid=%u, event.msg_id=%u, event.type=%u",
                event,
                (uint64_t)gni_cq_get_data(*event),
                (uint64_t)gni_cq_get_source(*event),
                (uint64_t)gni_cq_get_status(*event),
                (uint64_t)gni_cq_get_info(*event),
                (uint64_t)gni_cq_overrun(*event),
                (uint64_t)gni_cq_get_inst_id(*event),
                (uint64_t)gni_cq_get_tid(*event),
                (uint64_t)gni_cq_get_msg_id(*event),
                (uint64_t)gni_cq_get_type(*event));
    }
}

static void print_post_desc(
        const gni_post_descriptor_t *post_desc_ptr)
{
    if (post_desc_ptr != NULL) {
        log_debug(nnti_debug_level, "post_desc_ptr                  ==%p", (uint64_t)post_desc_ptr);
        log_debug(nnti_debug_level, "post_desc_ptr->next_descr      ==%p", (uint64_t)post_desc_ptr->next_descr);
        log_debug(nnti_debug_level, "post_desc_ptr->prev_descr      ==%p", (uint64_t)post_desc_ptr->prev_descr);

        log_debug(nnti_debug_level, "post_desc_ptr->post_id         ==%llu", (uint64_t)post_desc_ptr->post_id);
        log_debug(nnti_debug_level, "post_desc_ptr->status          ==%llu", (uint64_t)post_desc_ptr->status);
        log_debug(nnti_debug_level, "post_desc_ptr->cq_mode_complete==%llu", (uint64_t)post_desc_ptr->cq_mode_complete);

        log_debug(nnti_debug_level, "post_desc_ptr->type            ==%llu", (uint64_t)post_desc_ptr->type);
        log_debug(nnti_debug_level, "post_desc_ptr->cq_mode         ==%llu", (uint64_t)post_desc_ptr->cq_mode);
        log_debug(nnti_debug_level, "post_desc_ptr->dlvr_mode       ==%llu", (uint64_t)post_desc_ptr->dlvr_mode);
        log_debug(nnti_debug_level, "post_desc_ptr->local_addr      ==%llu", (uint64_t)post_desc_ptr->local_addr);
        log_debug(nnti_debug_level, "post_desc_ptr->remote_addr     ==%llu", (uint64_t)post_desc_ptr->remote_addr);
        log_debug(nnti_debug_level, "post_desc_ptr->length          ==%llu", (uint64_t)post_desc_ptr->length);
        log_debug(nnti_debug_level, "post_desc_ptr->rdma_mode       ==%llu", (uint64_t)post_desc_ptr->rdma_mode);
        log_debug(nnti_debug_level, "post_desc_ptr->src_cq_hndl     ==%llu", (uint64_t)post_desc_ptr->src_cq_hndl);
        log_debug(nnti_debug_level, "post_desc_ptr->sync_flag_value ==%llu", (uint64_t)post_desc_ptr->sync_flag_value);
        log_debug(nnti_debug_level, "post_desc_ptr->sync_flag_addr  ==%llu", (uint64_t)post_desc_ptr->sync_flag_addr);
        log_debug(nnti_debug_level, "post_desc_ptr->amo_cmd         ==%llu", (uint64_t)post_desc_ptr->amo_cmd);
        log_debug(nnti_debug_level, "post_desc_ptr->first_operand   ==%llu", (uint64_t)post_desc_ptr->first_operand);
        log_debug(nnti_debug_level, "post_desc_ptr->second_operand  ==%llu", (uint64_t)post_desc_ptr->second_operand);
        log_debug(nnti_debug_level, "post_desc_ptr->cqwrite_value   ==%llu", (uint64_t)post_desc_ptr->cqwrite_value);
    } else {
        log_debug(nnti_debug_level, "post_desc_ptr == NULL");
    }
}

static void print_gni_conn(gni_connection *c)
{
    int i=0;
    log_level debug_level=nnti_debug_level;

    log_debug(debug_level, "c->peer_name       =%s", c->peer_name);
    log_debug(debug_level, "c->peer_addr       =%u", c->peer_addr);
    log_debug(debug_level, "c->peer_port       =%u", (uint32_t)c->peer_port);
    log_debug(debug_level, "c->peer_instance   =%llu", (uint64_t)c->peer_instance);

    log_debug(debug_level, "c->state           =%d", c->state);
}

//static void print_put_buf(void *buf, uint32_t size)
//{
//    struct data_t {
//            uint32_t int_val;
//            float float_val;
//            double double_val;
//    };
//
//    struct  data_array_t {
//            u_int data_array_t_len;
//            struct data_t *data_array_t_val;
//    };
//
////    struct data_array_t *da=(struct data_array_t *)buf;
//    const struct data_t *array = (struct data_t *)buf;
//    const int len = size/sizeof(struct data_t);
//    int idx=0;
//
//    for (idx=0;idx<len;idx++) {
//        log_debug(nnti_select_debug_level, "array[%d].int_val=%u ; array[%d].float_val=%f ; array[%d].double_val=%f",
//                idx, array[idx].int_val,
//                idx, array[idx].float_val,
//                idx, array[idx].double_val);
//    }
//
//}

static void print_raw_buf(void *buf, uint32_t size)
{
    if (logging_debug(nnti_debug_level)) {
        FILE* f=logger_get_file();
        u_int64_t print_limit=(size<90) ? size : 90;
        fprintf(f, "\nbuf (%p)\n", buf);
        fflush(f);
        if (buf != NULL) {
            int l=0;
            for (l=0;l<print_limit;l++) {
                if (l%30 == 0) fprintf(f, "\nbuf (%lu) (offset(%d)) => ", buf, l);
                fprintf(f, "%02hhX", ((char *)buf)[l]);
            }
            fprintf(f, "\n");
        }
    }
}

static uint16_t get_dlvr_mode_from_env()
{
    char *mode=getenv("NSSI_GNI_DELIVERY_MODE");

    log_debug(nnti_debug_level, "NSSI_GNI_DELIVERY_MODE=%s", mode);
    if ((mode==NULL) || !strcmp(mode, "GNI_DLVMODE_PERFORMANCE")) {
        log_debug(nnti_debug_level, "setting delivery mode to GNI_DLVMODE_PERFORMANCE");
        return GNI_DLVMODE_PERFORMANCE;
    } else if (!strcmp(mode, "GNI_DLVMODE_IN_ORDER")) {
        log_debug(nnti_debug_level, "setting delivery mode to GNI_DLVMODE_IN_ORDER");
        return GNI_DLVMODE_IN_ORDER;
    } else if (!strcmp(mode, "GNI_DLVMODE_NO_ADAPT")) {
        log_debug(nnti_debug_level, "setting delivery mode to GNI_DLVMODE_NO_ADAPT");
        return GNI_DLVMODE_NO_ADAPT;
    } else if (!strcmp(mode, "GNI_DLVMODE_NO_HASH")) {
        log_debug(nnti_debug_level, "setting delivery mode to GNI_DLVMODE_NO_HASH");
        return GNI_DLVMODE_NO_HASH;
    } else if (!strcmp(mode, "GNI_DLVMODE_NO_RADAPT")) {
        log_debug(nnti_debug_level, "setting delivery mode to GNI_DLVMODE_NO_RADAPT");
        return GNI_DLVMODE_NO_RADAPT;
    } else {
        log_debug(nnti_debug_level, "defaulting delivery mode to GNI_DLVMODE_PERFORMANCE");
        return GNI_DLVMODE_PERFORMANCE;
    }
}
static int set_dlvr_mode(
        gni_post_descriptor_t *pd)
{
    pd->dlvr_mode=transport_global_data.delivery_mode;
//    log_debug(LOG_ALL, "pd->dlvr_mode=%X", pd->dlvr_mode);
}


static int post_wait(
        gni_cq_handle_t cq_hdl,
        int             timeout,
        int             retries)
{
    int rc=0;
    int i=0;
    trios_declare_timer(call_time);
    gni_post_descriptor_t *post_desc_ptr;
    gni_cq_entry_t ev_data;

    log_debug(nnti_ee_debug_level, "enter");

    memset(&ev_data, 0, sizeof(ev_data));
    for(i=0;i<=retries;i++) {
        log_debug(nnti_debug_level, "calling CqWaitEvent");
        trios_start_timer(call_time);
        rc=GNI_CqWaitEvent (cq_hdl, timeout, &ev_data);
        trios_stop_timer("post_wait - CqWaitEvent", call_time);
        if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqWaitEvent failed: %d", rc);
    }

    log_debug(nnti_debug_level, "calling GetComplete");
    trios_start_timer(call_time);
    rc=GNI_GetCompleted (cq_hdl, ev_data, &post_desc_ptr);
    trios_stop_timer("post_wait - GetCompleted", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "GetCompleted failed: %d", rc);
    print_post_desc(post_desc_ptr);

    log_debug(nnti_ee_debug_level, "exit");

    return(rc);
}


static int reset_req_index(
        gni_request_queue_handle  *req_queue_attrs)
{
    int rc=0;
    gni_post_descriptor_t  post_desc;

    uint64_t value_before_reset=0;
    uint64_t value_before_reset_addr=(uint64_t)&value_before_reset;
//    gni_cq_handle_t value_before_reset_mem_cq_hdl;
    gni_mem_handle_t value_before_reset_mem_hdl;
    gni_cq_handle_t reset_cq_hdl;
    gni_ep_handle_t reset_ep_hdl;

    log_debug(nnti_ee_debug_level, "enter");

    rc=GNI_CqCreate (transport_global_data.nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &reset_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate(value_before_reset_cq_hdl) failed: %d", rc);

//    rc=GNI_CqCreate (transport_global_data.nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &value_before_reset_mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate(value_before_reset_mem_cq_hdl) failed: %d", rc);

    rc=GNI_MemRegister (transport_global_data.nic_hdl, value_before_reset_addr, sizeof(uint64_t), NULL, GNI_MEM_READWRITE, -1, &value_before_reset_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemRegister(value_before_reset) failed: %d", rc);

    rc=GNI_EpCreate (transport_global_data.nic_hdl, reset_cq_hdl, &reset_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpCreate(reset_ep_hdl) failed: %d", rc);
    rc=GNI_EpBind (reset_ep_hdl, transport_global_data.alps_info.local_addr, transport_global_data.instance);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpBind(reset_ep_hdl) failed: %d", rc);

//    log_debug(nnti_debug_level, "index before reset(%llu).", req_queue_attrs->req_index);


    memset(&post_desc, 0, sizeof(gni_post_descriptor_t));
    post_desc.type           =GNI_POST_AMO;
    post_desc.cq_mode        =GNI_CQMODE_GLOBAL_EVENT;

    set_dlvr_mode(&post_desc);

    post_desc.local_addr     =value_before_reset_addr;
    post_desc.local_mem_hndl =value_before_reset_mem_hdl;
    post_desc.remote_addr    =req_queue_attrs->req_index_addr;
    post_desc.remote_mem_hndl=req_queue_attrs->req_index_mem_hdl;
    post_desc.length         =sizeof(uint64_t);
    post_desc.amo_cmd        =GNI_FMA_ATOMIC_FAND;
    post_desc.first_operand  =0;

    log_debug(nnti_debug_level, "calling PostFma(reset index ep_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            reset_ep_hdl, post_desc.local_addr, post_desc.remote_addr);
    rc=GNI_PostFma(reset_ep_hdl, &post_desc);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostFma(reset index) failed: %d", rc);

    post_wait(reset_cq_hdl, 1000, 0);

    rc=GNI_MemDeregister (transport_global_data.nic_hdl, &value_before_reset_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemDeregister(1) failed: %d", rc);

    rc=GNI_EpUnbind (reset_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpUnbind() failed: %d", rc);
    rc=GNI_EpDestroy (reset_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpDestroy() failed: %d", rc);

//    rc=GNI_CqDestroy (value_before_reset_mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);
    rc=GNI_CqDestroy (reset_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);

    req_queue_attrs->last_index_before_reset=value_before_reset;
    req_queue_attrs->req_processed_reset_limit=value_before_reset;
    log_debug(nnti_event_debug_level, "index before reset(%llu).", req_queue_attrs->last_index_before_reset);
    log_debug(nnti_event_debug_level, "index after reset(%llu).", req_queue_attrs->req_index);

    log_debug(nnti_ee_debug_level, "exit");

    return(0);
}

static int fetch_add_buffer_offset(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     addend,
        uint64_t                    *prev_offset)
{
    int rc=0;
    trios_declare_timer(call_time);
    gni_post_descriptor_t  post_desc;
//    gni_post_descriptor_t *post_desc_ptr;
    gni_cq_entry_t ev_data;
    int extras_absorbed=0;

    log_level debug_level=nnti_debug_level;

    static uint64_t last_offset=0;

    log_debug(nnti_ee_debug_level, "enter");

    memset(&post_desc, 0, sizeof(gni_post_descriptor_t));
    post_desc.type           =GNI_POST_AMO;
    post_desc.cq_mode        =GNI_CQMODE_GLOBAL_EVENT;

    set_dlvr_mode(&post_desc);

    post_desc.local_addr     =local_req_queue_attrs->req_index_addr;
    post_desc.local_mem_hndl =local_req_queue_attrs->req_index_mem_hdl;
    post_desc.remote_addr    =remote_req_queue_attrs->req_index_addr;
    post_desc.remote_mem_hndl=remote_req_queue_attrs->req_index_mem_hdl;
    post_desc.length         =sizeof(uint64_t);
    post_desc.amo_cmd        =GNI_FMA_ATOMIC_FADD;
    post_desc.first_operand  =addend;
    post_desc.second_operand =0;

    do {
        log_debug(debug_level, "calling PostFma(fetch add - req_index_ep_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
                local_req_queue_attrs->req_index_ep_hdl, post_desc.local_addr, post_desc.remote_addr);
        rc=GNI_PostFma(local_req_queue_attrs->req_index_ep_hdl, &post_desc);
        if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "PostFma(fetch add) failed: %d", rc);

        post_wait(local_req_queue_attrs->req_index_cq_hdl, 1000, 0);

        log_debug(debug_level, "fetched queue_index(%llu)", local_req_queue_attrs->req_index);
        *prev_offset=local_req_queue_attrs->req_index;
        if (*prev_offset >= remote_req_queue_attrs->req_count) {
            uint64_t  ui64;
            uint32_t *ptr32=NULL;


            log_debug(debug_level, "fetched queue_index(%llu) >= req_count(%llu)",
                    local_req_queue_attrs->req_index, remote_req_queue_attrs->req_count);

            memset(&ev_data, 0, sizeof(ev_data));
            do {
                log_debug(debug_level, "calling CqWaitEvent(unblock)");
                trios_start_timer(call_time);
                rc=GNI_CqWaitEvent (local_req_queue_attrs->unblock_mem_cq_hdl, 1000, &ev_data);
                trios_stop_timer("unblock", call_time);
                if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "CqWaitEvent(unblock) failed: %d", rc);
            } while (rc!=GNI_RC_SUCCESS);
            ui64=gni_cq_get_data(ev_data);
            ptr32=(uint32_t*)&ui64;
            ptr32++;
            log_debug(debug_level, "received unblock from server (ACKing requests <= %llu)", *ptr32);

            if (rc==GNI_RC_SUCCESS) {
                extras_absorbed=0;
                log_debug(debug_level, "calling CqGetEvent(absorb extra unblocks)");
                do {
                    log_debug(debug_level, "calling CqGetEvent(absorb extra unblocks)");
                    rc=GNI_CqGetEvent (local_req_queue_attrs->unblock_mem_cq_hdl, &ev_data);
                    if (rc==GNI_RC_SUCCESS) {
                        extras_absorbed++;

                        ui64=gni_cq_get_data(ev_data);
                        ptr32=(uint32_t*)&ui64;
                        ptr32++;
                        log_debug(debug_level, "received extra unblock from server (ACKing requests <= %llu)", *ptr32);
                    }
                } while (rc==GNI_RC_SUCCESS);
                if ((rc!=GNI_RC_SUCCESS) && (rc!=GNI_RC_NOT_DONE)) log_error(debug_level, "CqGetEvent(absorb extra unblocks) failed: %d", rc);
                log_debug(debug_level, "absorbed %d extra unblocks)", extras_absorbed);
            }
        } else if (*prev_offset < last_offset) {
            uint64_t  ui64;
            uint32_t *ptr32=NULL;

            /* absorb one unblock message */
            log_debug(nnti_event_debug_level, "calling CqGetEvent(absorb one unblock)");
            rc=GNI_CqWaitEvent (local_req_queue_attrs->unblock_mem_cq_hdl, 1000, &ev_data);
            if (rc==GNI_RC_SUCCESS) {
                ui64=gni_cq_get_data(ev_data);
                ptr32=(uint32_t*)&ui64;
                ptr32++;
                log_debug(nnti_event_debug_level, "received one unblock from server (ACKing requests <= %llu)", *ptr32);
            } else if (rc!=GNI_RC_NOT_DONE) {
                log_error(debug_level, "CqGetEvent(absorb one unblock) failed: %d", rc);
            }
        }
    } while (*prev_offset >= remote_req_queue_attrs->req_count);

    last_offset=*prev_offset;

    log_debug(nnti_ee_debug_level, "exit");

    return(0);
}

static int send_req(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     offset,
        const NNTI_buffer_t         *reg_buf)
{
    int rc=0;
    gni_post_descriptor_t  post_desc;

    trios_declare_timer(call_time);

    log_debug(nnti_ee_debug_level, "enter");

    memset(&post_desc, 0, sizeof(gni_post_descriptor_t));
#if defined(USE_FMA) || defined(USE_MIXED)
    post_desc.type                 =GNI_POST_FMA_PUT;
    post_desc.cq_mode              =GNI_CQMODE_GLOBAL_EVENT;
#elif defined(USE_RDMA)
    post_desc.type                 =GNI_POST_RDMA_PUT;
    post_desc.cq_mode              =GNI_CQMODE_LOCAL_EVENT;
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    set_dlvr_mode(&post_desc);

    post_desc.local_addr           =reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.buf;
    post_desc.local_mem_hndl.qword1=reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword1;
    post_desc.local_mem_hndl.qword2=reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.mem_hdl.qword2;
    post_desc.remote_addr          =remote_req_queue_attrs->req_buffer_addr+offset;
    post_desc.remote_mem_hndl      =remote_req_queue_attrs->req_mem_hdl;
    post_desc.length               =reg_buf->buffer_addr.NNTI_remote_addr_t_u.gni.size;

    print_raw_buf((void *)post_desc.local_addr, post_desc.length);

#if defined(USE_FMA) || defined(USE_MIXED)
    log_debug(nnti_debug_level, "calling PostFma(send req ep_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            local_req_queue_attrs->req_ep_hdl, post_desc.local_addr, post_desc.remote_addr);
    trios_start_timer(call_time);
    rc=GNI_PostFma(local_req_queue_attrs->req_ep_hdl, &post_desc);
    trios_stop_timer("PostFma req", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostFma(fma put) failed: %d", rc);
#elif defined(USE_RDMA)
    log_debug(nnti_debug_level, "calling PostRdma(send req ep_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            local_req_queue_attrs->req_ep_hdl, post_desc.local_addr, post_desc.remote_addr);
    trios_start_timer(call_time);
    rc=GNI_PostRdma(local_req_queue_attrs->req_ep_hdl, &post_desc);
    trios_stop_timer("PostRdma req", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostRdma(rdma put) failed: %d", rc);
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    post_wait(local_req_queue_attrs->req_cq_hdl, 1000, 0);

    log_debug(nnti_ee_debug_level, "exit");

    return(0);
}

static int send_wc(
        nnti_gni_client_queue       *local_req_queue_attrs,
        nnti_gni_server_queue_attrs *remote_req_queue_attrs,
        uint64_t                     offset,
        const NNTI_buffer_t         *reg_buf)
{
    int rc=0;
    gni_memory_handle *gni_mem_hdl=NULL;

    trios_declare_timer(call_time);

    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;
    assert(gni_mem_hdl);

    log_debug(nnti_ee_debug_level, "enter");

    memset(&gni_mem_hdl->post_desc, 0, sizeof(gni_post_descriptor_t));
#if defined(USE_FMA) || defined(USE_MIXED)
    gni_mem_hdl->post_desc.type           =GNI_POST_FMA_PUT;
    gni_mem_hdl->post_desc.cq_mode        =GNI_CQMODE_GLOBAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#elif defined(USE_RDMA)
    gni_mem_hdl->post_desc.type           =GNI_POST_RDMA_PUT;
    gni_mem_hdl->post_desc.cq_mode        =GNI_CQMODE_LOCAL_EVENT|GNI_CQMODE_REMOTE_EVENT;
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif

    set_dlvr_mode(&gni_mem_hdl->post_desc);

    gni_mem_hdl->post_desc.local_addr     =(uint64_t)&gni_mem_hdl->wc;
    gni_mem_hdl->post_desc.local_mem_hndl =gni_mem_hdl->wc_mem_hdl;
    gni_mem_hdl->post_desc.remote_addr    =remote_req_queue_attrs->wc_buffer_addr+offset;
    gni_mem_hdl->post_desc.remote_mem_hndl=remote_req_queue_attrs->wc_mem_hdl;
    gni_mem_hdl->post_desc.length         =sizeof(nnti_gni_work_completion);

    print_raw_buf((void *)gni_mem_hdl->post_desc.local_addr, gni_mem_hdl->post_desc.length);

    GNI_EpSetEventData(gni_mem_hdl->wc_ep_hdl, offset/sizeof(nnti_gni_work_completion), offset/sizeof(nnti_gni_work_completion));

#if defined(USE_FMA) || defined(USE_MIXED)
    log_debug(nnti_debug_level, "calling PostFma(send_wc wc_ep_hdl(%llu) wc_cq_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            gni_mem_hdl->wc_ep_hdl, gni_mem_hdl->wc_cq_hdl, gni_mem_hdl->post_desc.local_addr, gni_mem_hdl->post_desc.remote_addr);
    trios_start_timer(call_time);
    rc=GNI_PostFma(gni_mem_hdl->wc_ep_hdl, &gni_mem_hdl->post_desc);
    trios_stop_timer("PostFma wc", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostFma(fma put) failed: %d", rc);
#elif defined(USE_RDMA)
    log_debug(nnti_debug_level, "calling PostRdma(send_wc wc_ep_hdl(%llu) wc_cq_hdl(%llu), local_addr=%llu, remote_addr=%llu)",
            gni_mem_hdl->wc_ep_hdl, gni_mem_hdl->wc_cq_hdl, gni_mem_hdl->post_desc.local_addr, gni_mem_hdl->post_desc.remote_addr);
    trios_start_timer(call_time);
    rc=GNI_PostRdma(gni_mem_hdl->wc_ep_hdl, &gni_mem_hdl->post_desc);
    trios_stop_timer("PostRdma wc", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostRdma(rdma put) failed: %d", rc);
#else
#error Must define an RDMA method - USE_FMA or USE_RDMA or USE_MIXED
#endif


    log_debug(nnti_ee_debug_level, "exit");

    return(0);
}

static int request_send(
        nnti_gni_client_queue       *client_q,
        nnti_gni_server_queue_attrs *server_q,
        const NNTI_buffer_t         *reg_buf,
        int                          req_num)
{
    int rc=0;
    uint64_t offset=0;
    trios_declare_timer(call_time);

    gni_memory_handle *gni_mem_hdl=NULL;

    nnti_gni_work_completion  wc_buffer;
    uint32_t         wc_size       =sizeof(nnti_gni_work_completion);
    uint32_t         wc_count      =server_q->req_count;
    uint32_t         wc_buffer_size=wc_size*wc_count;

    gni_mem_hdl=(gni_memory_handle *)reg_buf->transport_private;
    assert(gni_mem_hdl);

    log_debug(nnti_ee_debug_level, "enter");

    log_debug(nnti_debug_level, "calling fetch_add_buffer_offset()");
    trios_start_timer(call_time);
    rc=fetch_add_buffer_offset(client_q, server_q, 1, &offset);
    trios_stop_timer("fetch_add_buffer_offset", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "fetch_add_buffer_offset() failed: %d", rc);

    log_debug(nnti_debug_level, "calling send_req()");
    trios_start_timer(call_time);
    rc=send_req(client_q, server_q, offset*server_q->req_size, reg_buf);
    trios_stop_timer("send_req", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "send_req() failed: %d", rc);

    gni_mem_hdl->wc.ack_received=0;
    gni_mem_hdl->wc.inst_id     =transport_global_data.instance;
    gni_mem_hdl->wc.op         =GNI_OP_SEND;
    gni_mem_hdl->wc.byte_len   =reg_buf->payload_size;
    gni_mem_hdl->wc.byte_offset=offset*server_q->req_size;
    gni_mem_hdl->wc.src_offset =0;
    gni_mem_hdl->wc.dest_offset=offset*server_q->req_size;

    log_debug(nnti_debug_level, "calling send_wc()");
    trios_start_timer(call_time);
    rc=send_wc(client_q, server_q, offset*wc_size, reg_buf);
    trios_stop_timer("send_wc", call_time);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "send_wc() failed: %d", rc);

    log_debug(nnti_ee_debug_level, "exit");

    return(0);
}

static int send_unblock(
        gni_request_queue_handle *local_req_queue_attrs)
{
    int rc=0;
    gni_post_descriptor_t  post_desc;
    uint32_t *ptr32=NULL;

    trios_declare_timer(call_time);

    log_debug(nnti_ee_debug_level, "enter");


    conn_by_inst_iter_t i;
    for (i=connections_by_instance.begin(); i != connections_by_instance.end(); i++) {

        //NNTI_instance_id key = i->first;
        gni_connection *conn = i->second;

        memset(&post_desc, 0, sizeof(gni_post_descriptor_t));
        post_desc.type           =GNI_POST_CQWRITE;
        post_desc.cq_mode        =GNI_CQMODE_GLOBAL_EVENT|GNI_CQMODE_REMOTE_EVENT;

        set_dlvr_mode(&post_desc);

        post_desc.remote_mem_hndl=conn->queue_remote_attrs.client.unblock_mem_hdl;
        ptr32=(uint32_t*)&post_desc.cqwrite_value;
        ptr32++;
        *ptr32=local_req_queue_attrs->total_req_processed;


        rc=GNI_EpBind (local_req_queue_attrs->unblock_ep_hdl, conn->peer_alps_info.local_addr, conn->peer_instance);
        if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpBind(reset_ep_hdl, inst_id=%llu) failed: %d", conn->peer_instance, rc);

        log_debug(nnti_debug_level, "calling PostCqWrite(send_unblock to instance(%llu))", conn->peer_instance);
        trios_start_timer(call_time);
        rc=GNI_PostCqWrite(local_req_queue_attrs->unblock_ep_hdl, &post_desc);
        trios_stop_timer("send_unblock", call_time);
        if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "PostCqWrite(send_unblock, inst_id=%llu) failed: %d", conn->peer_instance, rc);

        post_wait(local_req_queue_attrs->unblock_cq_hdl, 1000, 0);

        rc=GNI_EpUnbind (local_req_queue_attrs->unblock_ep_hdl);
        if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpUnbind(inst_id=%llu) failed: %d", conn->peer_instance, rc);
    }

    log_debug(nnti_ee_debug_level, "exit");

    return(0);
}


static int client_req_queue_init(
        gni_connection *c)
{
    int rc;

    nnti_gni_client_queue  *q              =&c->queue_local_attrs;
    alpsAppGni_t           *server_params  =&c->peer_alps_info;
    uint64_t                server_instance=c->peer_instance;
    uint64_t                req_size       =c->queue_remote_attrs.server.req_size;
    uint64_t                req_count      =c->queue_remote_attrs.server.req_count;


    q->req_index=0;
    q->req_index_addr=(uint64_t)&q->req_index;

    q->unblock_buffer=0;
    q->unblock_buffer_addr=(uint64_t)&q->unblock_buffer;

    rc=GNI_CqCreate (c->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->req_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);
    rc=GNI_CqCreate (c->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->req_index_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);

//    rc=GNI_CqCreate (c->nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->req_index_mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);
    rc=GNI_CqCreate (c->nic_hdl, 5, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->unblock_mem_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);


    rc=GNI_MemRegister (c->nic_hdl, q->req_index_addr, sizeof(uint64_t), NULL, GNI_MEM_READWRITE, -1, &q->req_index_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemRegister(1) failed: %d", rc);
    rc=GNI_MemRegister (c->nic_hdl, q->unblock_buffer_addr, sizeof(uint64_t), q->unblock_mem_cq_hdl, GNI_MEM_READWRITE, -1, &q->unblock_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemRegister(1) failed: %d", rc);


    rc=GNI_EpCreate (c->nic_hdl, q->req_cq_hdl, &q->req_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpCreate(req_ep_hdl) failed: %d", rc);
    rc=GNI_EpBind (q->req_ep_hdl, server_params->local_addr, server_instance);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpBind(req_ep_hdl) failed: %d", rc);

    rc=GNI_EpCreate (c->nic_hdl, q->req_index_cq_hdl, &q->req_index_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpCreate(req_index_ep_hdl) failed: %d", rc);
    rc=GNI_EpBind (q->req_index_ep_hdl, server_params->local_addr, server_instance);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpBind(req_index_ep_hdl) failed: %d", rc);

}

static int client_req_queue_destroy(
        gni_connection *c)
{
    int rc;
    log_level debug_level = nnti_debug_level;  // nnti_debug_level;

    log_debug(debug_level, "client_req_queue_destroy: start");

    nnti_gni_client_queue     *q=&c->queue_local_attrs;

    rc=GNI_MemDeregister (c->nic_hdl, &q->req_index_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "MemDeregister(1) failed: %d", rc);
    rc=GNI_MemDeregister (c->nic_hdl, &q->unblock_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "MemDeregister(1) failed: %d", rc);

//    rc=GNI_CqDestroy (q->req_index_mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);
    rc=GNI_CqDestroy (q->unblock_mem_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "CqDestroy() failed: %d", rc);

    rc=GNI_EpUnbind (q->req_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "EpUnbind() failed: %d", rc);
    rc=GNI_EpDestroy (q->req_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "EpDestroy() failed: %d", rc);

    rc=GNI_EpUnbind (q->req_index_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "EpUnbind() failed: %d", rc);
    rc=GNI_EpDestroy (q->req_index_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "EpDestroy() failed: %d", rc);

    rc=GNI_CqDestroy (q->req_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "CqDestroy() failed: %d", rc);
    rc=GNI_CqDestroy (q->req_index_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(debug_level, "CqDestroy() failed: %d", rc);

    log_debug(debug_level, "client_req_queue_destroy: exit");
    return(0);
}

static int server_req_queue_init(
        gni_request_queue_handle *q,
        char                     *buffer,
        uint64_t                  req_size,
        uint64_t                  req_count)
{
    int rc;
    int i;
    gni_memory_handle *gni_mem_hdl=(gni_memory_handle *)q->reg_buf->transport_private;


    q->req_buffer     =buffer;
    q->req_size       =req_size;
    q->req_count      =req_count;
    q->req_buffer_size=req_count*req_size;

    q->wc_buffer_size=q->req_count * sizeof(nnti_gni_work_completion);
    q->wc_buffer     =(nnti_gni_work_completion *)calloc(q->req_count, sizeof(nnti_gni_work_completion));

    q->req_index     =0;
    q->req_index_addr=(uint64_t)&q->req_index;

    q->req_processed=0;
    q->req_processed_reset_limit=q->req_count;

    rc=GNI_CqCreate (transport_global_data.nic_hdl, 1, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->unblock_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);
    rc=GNI_EpCreate (transport_global_data.nic_hdl, q->unblock_cq_hdl, &q->unblock_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpCreate(reset_ep_hdl) failed: %d", rc);

//    rc=GNI_CqCreate (transport_global_data.nic_hdl, req_count, 0, GNI_CQ_BLOCKING, NULL, NULL, &gni_mem_hdl->mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);
//    rc=GNI_CqCreate (transport_global_data.nic_hdl, req_count, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->req_index_mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);
    rc=GNI_CqCreate (transport_global_data.nic_hdl, req_count, 0, GNI_CQ_BLOCKING, NULL, NULL, &q->wc_mem_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqCreate() failed: %d", rc);

    rc=GNI_MemRegister (transport_global_data.nic_hdl, (uint64_t)q->req_buffer, q->req_buffer_size, NULL, GNI_MEM_READWRITE, -1, &gni_mem_hdl->mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemRegister(1) failed: %d", rc);
    rc=GNI_MemRegister (transport_global_data.nic_hdl, q->req_index_addr, sizeof(uint64_t), NULL, GNI_MEM_READWRITE, -1, &q->req_index_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemRegister(1) failed: %d", rc);
    rc=GNI_MemRegister (transport_global_data.nic_hdl, (uint64_t)q->wc_buffer, q->wc_buffer_size, q->wc_mem_cq_hdl, GNI_MEM_READWRITE, -1, &q->wc_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemRegister(1) failed: %d", rc);
}

static int server_req_queue_destroy(
        gni_request_queue_handle *q)
{
    int rc;
    int i;
    gni_memory_handle *gni_mem_hdl=(gni_memory_handle *)q->reg_buf->transport_private;

    rc=GNI_MemDeregister (transport_global_data.nic_hdl, &gni_mem_hdl->mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemDeregister(1) failed: %d", rc);
    rc=GNI_MemDeregister (transport_global_data.nic_hdl, &q->req_index_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemDeregister(1) failed: %d", rc);
    rc=GNI_MemDeregister (transport_global_data.nic_hdl, &q->wc_mem_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "MemDeregister(1) failed: %d", rc);

//    rc=GNI_CqDestroy (gni_mem_hdl->mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);
//    rc=GNI_CqDestroy (q->req_index_mem_cq_hdl);
//    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);
    rc=GNI_CqDestroy (q->wc_mem_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);

    rc=GNI_EpDestroy (q->unblock_ep_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "EpDestroy() failed: %d", rc);
    rc=GNI_CqDestroy (q->unblock_cq_hdl);
    if (rc!=GNI_RC_SUCCESS) log_error(nnti_debug_level, "CqDestroy() failed: %d", rc);

//    free(q->req_buffer);
    free(q->wc_buffer);
}

#define LISTEN_IFACE_BASENAME "ipog"
static NNTI_result_t get_ipaddr(
        char *ipaddr,
        int maxlen)
{
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        ipaddr[0]='\0';
        if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            log_debug(nnti_debug_level, "checking iface (IPv4) name (%s)", ifa->ifa_name);
            inet_ntop(AF_INET, tmpAddrPtr, ipaddr, maxlen);
            log_debug(nnti_debug_level, "hostname(%s) has IP Address %s", ifa->ifa_name, ipaddr);
            if (0==strncmp(ifa->ifa_name, LISTEN_IFACE_BASENAME, strlen(LISTEN_IFACE_BASENAME))) {
                log_debug(nnti_debug_level, "hostname(%s) matches", ifa->ifa_name);
                break;
            }
        } else if (ifa->ifa_addr->sa_family==AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            log_debug(nnti_debug_level, "checking iface (IPv6) name (%s)", ifa->ifa_name);
            inet_ntop(AF_INET6, tmpAddrPtr, ipaddr, maxlen);
            log_debug(nnti_debug_level, "hostname(%s) has IP Address %s", ifa->ifa_name, ipaddr);
            if (0==strncmp(ifa->ifa_name, LISTEN_IFACE_BASENAME, strlen(LISTEN_IFACE_BASENAME))) {
                log_debug(nnti_debug_level, "hostname(%s) matches", ifa->ifa_name);
                break;
            }
        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

    log_debug(nnti_debug_level, "ipaddr(%s)", ipaddr);
    if (ipaddr[0]=='\0') {
        return NNTI_ENOENT;
    } else {
        return NNTI_OK;
    }
}
