/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 central and MIDI packet queues adapted from VIRGO / GRANA. */
#if !defined(GRANA_WEB) && !defined(DINGHY_HOST)
#define BLE_DEVICE_NAME "Mariotronics"
#define FELUCCA_BLE_CENTRAL 1
#define RING_PUBLISH_BT() __asm__ volatile("" ::: "memory")
static struct { uint8_t flat; } dualcfg;
#include "ble/ble_stack.c"
#include "ble/ble_vm.c"
#define BTQ 64u
static uint32_t bt_in_q[BTQ], bt_out_q[BTQ], bt_out_ms[BTQ];
static volatile uint32_t bti_w, bti_r, bto_w, bto_r;
static volatile uint8_t bt_out_on, bt_lost;
static uint8_t bt_was_conn, bt_up, radio_on, scanning;
static uint32_t scan_at;
static struct ble_scan_tab bt_found;
static const char *bt_status="Radio off";
BLE_API void ble_app_state(void)                /* the BLE interrupts, or the main loop with them held */
{
    uint8_t c = (uint8_t)ble_connected();
    bt_out_on = (uint8_t)ble_midi_ready();
    if (!bt_out_on) bto_r = bto_w;
    if (bt_was_conn && !c) bt_lost = 1;
    bt_was_conn = c;
}
BLE_API int ble_app_midi_peek(uint32_t *pkt, uint32_t *t_ms)
{
    if (bto_r == bto_w) return 0;
    RING_PUBLISH_BT();
    *pkt = bt_out_q[bto_r % BTQ];
    *t_ms = bt_out_ms[bto_r % BTQ];
    return 1;
}
BLE_API void ble_app_midi_pop(void) { RING_PUBLISH_BT(); bto_r++; }
BLE_API void ble_app_bond(const uint8_t rand[8], uint16_t ediv, const uint8_t ltk[16]) { (void)rand, (void)ediv, (void)ltk; }   /* (a computer bonding to GRANA: not kept) */
BLE_API void ble_app_peer_id(const uint8_t irk[16], const uint8_t addr[6], uint8_t addr_rand) { (void)irk, (void)addr, (void)addr_rand; }
BLE_API void ble_app_central_keys(const struct ble_keys *keys) { (void)keys; }
BLE_API void ble_app_midi_in(uint32_t pkt, uint16_t ts, uint16_t last_ts)
{
    uint32_t cin = pkt & 15u, st = (pkt >> 8) & 0xFFu;
    (void)ts, (void)last_ts;
    if (!(cin >= 8u && cin <= 0xEu) && !(cin == 0xFu && (st == 0xF8u || (st >= 0xFAu && st <= 0xFCu)))) return;
    if (bti_w - bti_r >= BTQ) return;
    bt_in_q[bti_w % BTQ] = pkt;
    RING_PUBLISH_BT();
    bti_w++;
}
static int bt_send(uint32_t st, uint32_t d1, uint32_t d2)   /* the main loop: one message to the controller -> 1 queued */
{
    if (!bt_out_on || bto_w - bto_r >= BTQ - 2u) return 0;
    bt_out_q[bto_w % BTQ] = (st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
    bt_out_ms[bto_w % BTQ] = fm1_ms;
    RING_PUBLISH_BT();
    bto_w++;
    return 1;
}


static int radio_read(void *ctx,uint32_t pos,uint8_t *dst,uint32_t n) {
    (void)ctx; return flash_ok ? st_read(pos,dst,n) : -1;
}
static int radio_power(int on) {
    if (on && !bt_up) {
        struct ble_vm_info vm;
        struct ble_rf_trims trims;
        if (!ble_vm_scan(radio_read,0,&vm,&trims,0,0)) {bt_status="No RF calibration";return 0;}
        uint8_t addr[6];
        uint32_t irq=irq_save();
        fm1_guard_unlock_top();ble_hw_wl82_attach();fm1_guard_lock_top();
        int random=ble_hw_addr(addr);ble_init(addr,random);ble_hw_wl82_start(&trims);
        irq_restore(irq);bt_up=1;
    }
    if (bt_up) {
        fm1_ble_irqs_hold(1);
        if (!on) {ble_ll_scan(0);ble_central_cancel();scanning=0;bti_r=bti_w;bt_lost=1;}
        ble_enable(on);fm1_ble_irqs_hold(0);
    }
    radio_on=on;bt_status=on?"Radio on":"Radio off";return on;
}
static const struct ble_found *radio_peer(int row) {
    uint8_t idx[BLE_SCAN_N]; unsigned count=ble_scan_list(&bt_found,idx);
    return row>0 && (unsigned)row<=count ? &bt_found.e[idx[row-1]] : 0;
}
static void radio_action(int row) {
    if (!radio_on) return;
    if (!row) {
        fm1_ble_irqs_hold(1);ble_central_cancel();ble_ll_scan(1);fm1_ble_irqs_hold(0);
        ble_scan_clear(&bt_found);scanning=1;scan_at=fm1_ms;bt_status="Scanning";return;
    }
    const struct ble_found *dev=radio_peer(row);
    if (!dev) return;
    struct ble_peer peer={0};for (int i=0;i<6;i++) peer.addr[i]=dev->addr[i];
    peer.addr_rand=dev->addr_rand;
    fm1_ble_irqs_hold(1);ble_ll_scan(0);ble_central_cancel();ble_central_connect(&peer);fm1_ble_irqs_hold(0);
    scanning=0;bt_status="Connecting";
}
static const char *radio_name(int row) {
    const struct ble_found *d=radio_peer(row);
    return !row?"Scan for devices":!d?"Empty":d->name[0]?d->name:"Unnamed device";
}
static void radio_poll(void) {
    if (!radio_on) return;
    if (scanning) {
        uint8_t pdu[39];uint16_t rssi;unsigned n;
        while ((n=ble_ll_scan_take(pdu,&rssi))) ble_scan_add(&bt_found,pdu,n,rssi,fm1_ms);
        if (fm1_ms-scan_at>=10000) {fm1_ble_irqs_hold(1);ble_ll_scan(0);fm1_ble_irqs_hold(0);scanning=0;bt_status="Scan complete";}
    }
    if (ble_midi_ready()) bt_status="MIDI connected";
    else if (!scanning && ble_central_state()==BLE_CS_IDLE && ble_central_fail()) bt_status="Disconnected";
}
#endif
