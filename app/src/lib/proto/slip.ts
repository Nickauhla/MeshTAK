// -----------------------------------------------------------------------------
// Cadrage SLIP (RFC 1055) du lien BLE. Miroir de firmware/src/slip.h.
// -----------------------------------------------------------------------------
import { MAX_FRAME } from './frames.ts';

export const END = 0xc0;
export const ESC = 0xdb;
export const ESC_END = 0xdc;
export const ESC_ESC = 0xdd;

export function slipEncode(data: Uint8Array): Uint8Array {
  const out = new Uint8Array(2 + data.length * 2);
  let o = 0;
  out[o++] = END;
  for (let i = 0; i < data.length; i++) {
    const b = data[i];
    if (b === END) {
      out[o++] = ESC;
      out[o++] = ESC_END;
    } else if (b === ESC) {
      out[o++] = ESC;
      out[o++] = ESC_ESC;
    } else {
      out[o++] = b;
    }
  }
  out[o++] = END;
  return out.slice(0, o);
}

// Décodeur en flux : les notifications BLE arrivent fragmentées, on pousse les
// octets au fur et à mesure et le callback est appelé une fois par trame.
export class SlipDecoder {
  private buf = new Uint8Array(MAX_FRAME);
  private len = 0;
  private esc = false;
  private overflow = false;
  private onFrame: (frame: Uint8Array) => void;

  constructor(onFrame: (frame: Uint8Array) => void) {
    this.onFrame = onFrame;
  }

  reset(): void {
    this.len = 0;
    this.esc = false;
    this.overflow = false;
  }

  feed(chunk: Uint8Array): void {
    for (let i = 0; i < chunk.length; i++) this.push(chunk[i]);
  }

  private push(byte: number): void {
    let b = byte;
    if (b === END) {
      if (this.len > 0 && !this.overflow) this.onFrame(this.buf.slice(0, this.len));
      this.reset();
      return;
    }
    if (this.esc) {
      this.esc = false;
      if (b === ESC_END) b = END;
      else if (b === ESC_ESC) b = ESC;
      else {
        this.overflow = true; // séquence d'échappement invalide
        return;
      }
    } else if (b === ESC) {
      this.esc = true;
      return;
    }
    if (this.len >= MAX_FRAME) {
      this.overflow = true;
      return;
    }
    this.buf[this.len++] = b;
  }
}
