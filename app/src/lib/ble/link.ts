// -----------------------------------------------------------------------------
// Lien BLE avec le T-Beam : Nordic UART Service + cadrage SLIP.
// Le plugin @capacitor-community/bluetooth-le fournit une implémentation NATIVE
// sur iOS (CoreBluetooth) et Android, et retombe sur Web Bluetooth dans Chrome
// desktop — d'où une seule base de code pour les trois cibles.
// -----------------------------------------------------------------------------
import { BleClient, type ScanResult } from '@capacitor-community/bluetooth-le';

import { decodeFrame, encodeFrame, type Frame } from '../proto/frames.ts';
import { SlipDecoder, slipEncode } from '../proto/slip.ts';

export const NUS_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
export const NUS_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // app -> device
export const NUS_TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // device -> app

// Taille de fragment prudente : le MTU par défaut du BLE ne garantit que 20
// octets de charge utile par écriture.
const WRITE_CHUNK = 20;

function toDataView(bytes: Uint8Array): DataView {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  return new DataView(copy.buffer);
}

class BleLink {
  private deviceId = '';
  private decoder: SlipDecoder;
  private connectedFlag = false;

  onFrame: ((f: Frame) => void) | null = null;
  onConnectionChange: ((connected: boolean, deviceName: string) => void) | null = null;

  constructor() {
    this.decoder = new SlipDecoder((raw) => {
      const frame = decodeFrame(raw);
      if (frame && this.onFrame) this.onFrame(frame);
    });
  }

  get connected(): boolean {
    return this.connectedFlag;
  }

  /** Ouvre le sélecteur de périphérique, se connecte et s'abonne aux notifications. */
  async connect(): Promise<string> {
    await BleClient.initialize({ androidNeverForLocation: true });

    const device = await BleClient.requestDevice({
      services: [NUS_SERVICE],
      namePrefix: 'MeshRadio',
      optionalServices: [NUS_SERVICE],
    });

    await BleClient.connect(device.deviceId, () => this.handleDisconnect());
    this.deviceId = device.deviceId;
    this.decoder.reset();

    await BleClient.startNotifications(device.deviceId, NUS_SERVICE, NUS_TX, (value) => {
      this.decoder.feed(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
    });

    this.connectedFlag = true;
    const name = device.name ?? device.deviceId;
    this.onConnectionChange?.(true, name);
    return name;
  }

  async disconnect(): Promise<void> {
    if (!this.deviceId) return;
    try {
      await BleClient.disconnect(this.deviceId);
    } finally {
      this.handleDisconnect();
    }
  }

  /** Encode, cadre en SLIP et écrit la trame par fragments. */
  async send(frame: Partial<Frame> & { type: number }): Promise<void> {
    if (!this.connectedFlag) throw new Error('non connecté');
    const bytes = slipEncode(encodeFrame(frame));
    for (let off = 0; off < bytes.length; off += WRITE_CHUNK) {
      const chunk = bytes.slice(off, off + WRITE_CHUNK);
      await BleClient.writeWithoutResponse(this.deviceId, NUS_SERVICE, NUS_RX, toDataView(chunk));
    }
  }

  /** Scan passif — utile pour un écran de diagnostic. */
  async scan(durationMs = 4000): Promise<ScanResult[]> {
    await BleClient.initialize({ androidNeverForLocation: true });
    const found: ScanResult[] = [];
    await BleClient.requestLEScan({ services: [NUS_SERVICE] }, (r) => found.push(r));
    await new Promise((resolve) => setTimeout(resolve, durationMs));
    await BleClient.stopLEScan();
    return found;
  }

  private handleDisconnect(): void {
    this.connectedFlag = false;
    this.deviceId = '';
    this.decoder.reset();
    this.onConnectionChange?.(false, '');
  }
}

export const link = new BleLink();
