#include <assert.h>
#include <dpl/dpl.h>
#include <hal/hal_gpio.h>
#include <bsp/bsp.h>

/* BLE */
#include "mesh/mesh.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "mesh/glue.h"

#include <message/mesh_msg.h>
#include <gateway/ble_mesh/mesh_define.h>
#include <gateway/gateway/gateway.h>

#define MBUF_PKTHDR_OVERHEAD    (sizeof(struct os_mbuf_pkthdr) + 0)
#define MBUF_MEMBLOCK_OVERHEAD  (sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD)

#define MBUF_NUM_MBUFS      (20)
#define MBUF_PAYLOAD_SIZE   (sizeof(msg_rtls_t))
#define MBUF_BUF_SIZE       OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static struct os_mbuf_pool g_mbuf_pool;
static struct os_mempool g_mbuf_mempool;
static os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

static void
rtls_model_set(struct bt_mesh_model *model,
              struct bt_mesh_msg_ctx *ctx,
              struct os_mbuf *buf)
{  
    printf("rtls_model_set()\n");
}

static void
rtls_model_status(struct bt_mesh_model *model,
              struct bt_mesh_msg_ctx *ctx,
              struct os_mbuf *buf)
{
    hal_gpio_write(LED_UPLINK_0, LED_ON);

    int rc;
    struct os_mbuf *om;
    struct os_mqueue *mqueue;
    struct os_eventq *event;
    msg_rtls_t msg_rtls;

    om = os_mbuf_get_pkthdr(&g_mbuf_pool, 0);
    if (om) {
        msg_parse_rtls(buf, &msg_rtls);
        msg_rtls.opcode = BT_MESH_MODEL_OP_STATUS;
        msg_rtls.mesh_address = ctx->addr;
        msg_prepr_rtls_pipe(om, &msg_rtls);
        get_ble_to_net_mqueue_eventq(&mqueue, &event);
        rc = os_mqueue_put(mqueue, event, om);
        if (rc != 0) {
            printf("Unable to put mqueue: %d\n", rc);
        }
    }
    hal_gpio_write(LED_UPLINK_0, LED_OFF);
}

static const struct bt_mesh_model_op rtls_op[] = {
    { BT_MESH_MODEL_OP_SET, 0, rtls_model_set},
    { BT_MESH_MODEL_OP_STATUS, 0, rtls_model_status},
    BT_MESH_MODEL_OP_END,
};

static struct bt_mesh_cfg_srv cfg_srv = {
    .relay = BT_MESH_RELAY_DISABLED,
    .beacon = BT_MESH_BEACON_ENABLED,
    .frnd = BT_MESH_FRIEND_DISABLED,
    .gatt_proxy = BT_MESH_GATT_PROXY_ENABLED,

    /* 7 transmissions with 20ms interval */
    .default_ttl = 7,
    .net_transmit = BT_MESH_TRANSMIT(7, 20),
    .relay_retransmit = BT_MESH_TRANSMIT(7, 20),
};
static struct bt_mesh_model_pub model_pub_rtls;

struct bt_mesh_model model_root[] = {
    BT_MESH_MODEL_CFG_SRV(&cfg_srv),
    BT_MESH_MODEL(BT_MESH_MODEL_ID_RTLS, rtls_op, &model_pub_rtls, NULL),
};

static struct os_task g_gateway_task;
static os_stack_t g_task_gateway_stack[MYNEWT_VAL(APP_GATEWAY_TASK_STACK_SIZE)];
static struct os_mqueue mqueue_net_to_ble;
static struct os_eventq eventq_net_to_ble;

static void
process_net_to_ble_queue(struct os_event *ev)
{
    struct os_mbuf *om;
    struct os_mqueue *mqueue = (struct os_mqueue *)ev->ev_arg;
    struct bt_mesh_model *model = &model_root[1];
    msg_rtls_t msg_rtls;
    int rc;

    while ((om = os_mqueue_get(mqueue)) != NULL) {
        hal_gpio_write(LED_DOWNLINK_1, LED_ON);
        if(model->pub->addr != BT_MESH_ADDR_UNASSIGNED) {
            msg_parse_rtls_pipe(om, &msg_rtls);
            msg_print_rtls(&msg_rtls);
            msg_prepr_rtls(&model->pub->msg, &msg_rtls);
            rc = bt_mesh_model_publish(model);
            if(rc){
                printf("Unable to publish message from net to ble\n");
            }
            else{
                printf("NET->BLE\n");
            }
            os_mbuf_free_chain(model->pub->msg);
        }
        os_mbuf_free_chain(om);
        hal_gpio_write(LED_DOWNLINK_1, LED_OFF);
    }
}

static void
task_rtls_func(void *arg){
    while (1) {
        os_eventq_run(&eventq_net_to_ble);
    }
}

void 
get_net_to_ble_mqueue_eventq(struct os_mqueue **mqueue, struct os_eventq **eventq){
    *mqueue = &mqueue_net_to_ble;
    *eventq = &eventq_net_to_ble;
}

void model_gateway_init(){
    int rc;
    
    hal_gpio_init_out(LED_UPLINK_0, LED_OFF);
    hal_gpio_init_out(LED_DOWNLINK_1, LED_OFF);

    os_mqueue_init(&mqueue_net_to_ble, process_net_to_ble_queue, &mqueue_net_to_ble);
    os_eventq_init(&eventq_net_to_ble);

    os_task_init(&g_gateway_task, "mgw",
                task_rtls_func,
                NULL,
                MYNEWT_VAL(APP_GATEWAY_TASK_PRIORITY), 
                OS_WAIT_FOREVER,
                g_task_gateway_stack,
                MYNEWT_VAL(APP_GATEWAY_TASK_STACK_SIZE));

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS,
                          MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "mbuf_pool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE,
                           MBUF_NUM_MBUFS);
    assert(rc == 0);
}