#pragma once
// -----------------------------------------------------------------------------
// Cadrage SLIP (RFC 1055) du lien BLE. Miroir de app/src/lib/proto/slip.ts.
// -----------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

namespace slip {

static const uint8_t END = 0xC0;
static const uint8_t ESC = 0xDB;
static const uint8_t ESC_END = 0xDC;
static const uint8_t ESC_ESC = 0xDD;

// Encode `len` octets ; renvoie la taille écrite (0 si `cap` insuffisant).
// Pire cas : 2 + 2 × len.
inline size_t encode(const uint8_t *in, size_t len, uint8_t *out, size_t cap) {
    if (cap < 2 + 2 * len) return 0;
    size_t o = 0;
    out[o++] = END;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == END) {
            out[o++] = ESC;
            out[o++] = ESC_END;
        } else if (in[i] == ESC) {
            out[o++] = ESC;
            out[o++] = ESC_ESC;
        } else {
            out[o++] = in[i];
        }
    }
    out[o++] = END;
    return o;
}

// Décodeur en flux : on pousse les octets reçus, il rappelle `onFrame` par trame.
class Decoder {
  public:
    typedef void (*FrameCb)(const uint8_t *data, size_t len);

    explicit Decoder(FrameCb cb) : _cb(cb), _len(0), _esc(false), _overflow(false) {}

    void feed(const uint8_t *data, size_t n) {
        for (size_t i = 0; i < n; i++) push(data[i]);
    }

    void reset() {
        _len = 0;
        _esc = false;
        _overflow = false;
    }

  private:
    void push(uint8_t b) {
        if (b == END) {
            if (_len > 0 && !_overflow && _cb) _cb(_buf, _len);
            reset();
            return;
        }
        if (_esc) {
            _esc = false;
            if (b == ESC_END) b = END;
            else if (b == ESC_ESC) b = ESC;
            else {  // séquence d'échappement invalide : on jette la trame
                _overflow = true;
                return;
            }
        } else if (b == ESC) {
            _esc = true;
            return;
        }
        if (_len >= proto::MAX_FRAME) {
            _overflow = true;  // trame trop longue : on attend le prochain END
            return;
        }
        _buf[_len++] = b;
    }

    FrameCb _cb;
    uint8_t _buf[proto::MAX_FRAME];
    size_t _len;
    bool _esc;
    bool _overflow;
};

}  // namespace slip
