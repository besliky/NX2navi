// nn::web overrides: the web applet. The game opens web pages through nn::web::ShowWebPage (its Zelda Notes entry),
// which on the console starts the system's browser as a library applet and waits for it. The host has no library
// applets: the SDK's path asked the applet proxy for one, got no object back and dereferenced null (an access
// violation in lifted SDK code). ShowWebPage is answered here instead: the page is reported closed with the exit
// button at once, the game's call succeeds and play goes on, and the log names the page that was asked for.
//
// Layouts (nn::web, read from the SDK's own accessors, GetWebExitReason and the ShowWebPageArg constructor): both
// the argument and the return value are 0x2000-byte TLV storages. Header: u16 entry count, u16 pad, u32 shim kind;
// then entries of {u16 type, u16 size, u32 pad, data[size]} packed without alignment. Argument type 1 is the request
// URL (a string of at most 0xc00 bytes). Return value type 1 is the exit reason (u32): 0 exit button, 1 back button,
// 2 requested, 3 last URL, 7 error dialog; a storage without one reads as 0x14 (unknown).
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "host.h"

#define WEB_STORAGE 0x2000u
#define WEB_ARG_URL 1u
#define WEB_RET_EXIT_REASON 1u
#define WEB_EXIT_BUTTON 0u

// The entry of one type in a TLV storage: a pointer to its data and its size, or NULL.
static const uint8_t *web_tlv_find(const uint8_t *storage, unsigned type, unsigned *size) {
    unsigned n = (unsigned)storage[0] | (unsigned)storage[1] << 8;
    unsigned off = 8;
    for (unsigned i = 0; i < n && off + 8 <= WEB_STORAGE; i++) {
        unsigned t = (unsigned)storage[off] | (unsigned)storage[off + 1] << 8;
        unsigned sz = (unsigned)storage[off + 2] | (unsigned)storage[off + 3] << 8;
        if (off + 8 + sz > WEB_STORAGE) return NULL;
        if (t == type) { *size = sz; return storage + off + 8; }
        off += 8 + sz;
    }
    return NULL;
}

// Result ShowWebPage(WebPageReturnValue *out, const ShowWebPageArg &arg)
static void ov_show_web_page(Cpu *c) {
    uint8_t *out = arg_p(c, 0);
    const uint8_t *arg = arg_p(c, 1);
    char url[256] = "(no URL)";
    if (arg) {
        unsigned n = 0;
        const uint8_t *p = web_tlv_find(arg, WEB_ARG_URL, &n);
        if (p) {
            unsigned len = 0;
            while (len < n && len < sizeof url - 1 && p[len]) len++;
            memcpy(url, p, len);
            url[len] = 0;
        }
    }
    char lr[96];
    rt_log("web: nn::web::ShowWebPage(%s) from %s: the host has no web browser applet, so the page is reported closed "
           "(exit button) and the game goes on", url, ldr_describe(c->x[30] - 4, lr, sizeof lr));
    if (out) {
        memset(out, 0, WEB_STORAGE);
        out[0] = 1;                              // one entry
        out[8] = (uint8_t)WEB_RET_EXIT_REASON;   // type
        out[10] = 4;                             // size
        uint32_t reason = WEB_EXIT_BUTTON;
        memcpy(out + 16, &reason, 4);
    }
    ret_x(c, 0);
}
HOST_OVERRIDE("_ZN2nn3web11ShowWebPageEPNS0_18WebPageReturnValueERKNS0_14ShowWebPageArgE", ov_show_web_page);
