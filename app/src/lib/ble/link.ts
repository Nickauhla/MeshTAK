// -----------------------------------------------------------------------------
// Lien BLE avec le T-Beam : Nordic UART Service + cadrage SLIP.
// Le plugin @capacitor-community/bluetooth-le fournit une implémentation NATIVE
// sur iOS (CoreBluetooth) et Android, et retombe sur Web Bluetooth dans Chrome
// desktop — d'où une seule base de code pour les trois cibles.
//
// Le lien se RATTRAPE tout seul. Sur le terrain, le téléphone reste en poche et
// le boîtier au harnais : la liaison tombe pour un rien (portée, veille iOS,
// batterie). Une reconnexion manuelle à chaque coupure n'est pas tenable, donc
// tant que l'utilisateur n'a pas explicitement coupé, on rappelle le boîtier.
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

/** Boîtier retenu d'une session à l'autre : la reprise se fait sans sélecteur. */
const REMEMBERED_KEY = 'meshradio.device';

// Attente croissante puis plafonnée : on ne veut ni marteler la radio du
// téléphone, ni laisser passer plus de quinze secondes avant un nouvel essai.
const BACKOFF_MS = [700, 1500, 3000, 6000, 10000, 15000];

/** `retrying` = lien perdu, on rappelle ; `connecting` = tentative en cours. */
export type LinkState = 'off' | 'connecting' | 'retrying' | 'connected';

function toDataView(bytes: Uint8Array): DataView {
  const copy = new Uint8Array(bytes.length);
  copy.set(bytes);
  return new DataView(copy.buffer);
}

function remember(deviceId: string, name: string): void {
  try {
    localStorage.setItem(REMEMBERED_KEY, JSON.stringify({ deviceId, name }));
  } catch {
    /* stockage indisponible : on perd juste la reprise automatique */
  }
}

function recall(): { deviceId: string; name: string } | null {
  try {
    const raw = localStorage.getItem(REMEMBERED_KEY);
    if (!raw) return null;
    const v = JSON.parse(raw) as { deviceId?: string; name?: string };
    return v.deviceId ? { deviceId: v.deviceId, name: v.name ?? '' } : null;
  } catch {
    return null;
  }
}

class BleLink {
  private deviceId = '';
  private deviceName = '';
  private decoder: SlipDecoder;
  private connectedFlag = false;

  /** Vrai tant que l'utilisateur veut le lien : c'est ce qui autorise les rappels. */
  private wanted = false;
  private attempt = 0;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private opening = false;
  private hooked = false;

  onFrame: ((f: Frame) => void) | null = null;
  onConnectionChange: ((connected: boolean, deviceName: string) => void) | null = null;
  onStateChange: ((state: LinkState, attempt: number) => void) | null = null;

  constructor() {
    this.decoder = new SlipDecoder((raw) => {
      const frame = decodeFrame(raw);
      if (frame && this.onFrame) this.onFrame(frame);
    });
  }

  get connected(): boolean {
    return this.connectedFlag;
  }

  /** Un boîtier a déjà été appairé : la reprise peut se faire sans sélecteur. */
  get hasRemembered(): boolean {
    return recall() !== null;
  }

  /** Ouvre le sélecteur de périphérique, se connecte et s'abonne aux notifications. */
  async connect(): Promise<string> {
    this.wanted = true;
    this.installWakeHooks();
    this.emit('connecting');

    await BleClient.initialize({ androidNeverForLocation: true });
    const device = await BleClient.requestDevice({
      services: [NUS_SERVICE],
      namePrefix: 'MeshRadio',
      optionalServices: [NUS_SERVICE],
    });

    const name = await this.open(device.deviceId, device.name ?? device.deviceId);
    remember(device.deviceId, name);
    return name;
  }

  /**
   * Reprise silencieuse au démarrage de l'app, sans sélecteur ni geste de
   * l'utilisateur. Renvoie faux si aucun boîtier n'a jamais été appairé.
   */
  resume(): boolean {
    const saved = recall();
    if (!saved) return false;

    this.deviceId = saved.deviceId;
    this.deviceName = saved.name;
    this.wanted = true;
    this.installWakeHooks();
    void this.attemptReconnect();
    return true;
  }

  /** Coupure volontaire : plus aucun rappel, et le boîtier est oublié. */
  async disconnect(): Promise<void> {
    this.wanted = false;
    this.clearTimer();
    try {
      localStorage.removeItem(REMEMBERED_KEY);
    } catch {
      /* sans importance */
    }
    const id = this.deviceId;
    if (!id) {
      this.emit('off');
      return;
    }
    try {
      await BleClient.disconnect(id);
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

  /** Force un essai immédiat : retour au premier plan, bouton « réessayer ». */
  retryNow(): void {
    if (!this.wanted || this.connectedFlag) return;
    this.clearTimer();
    this.attempt = 0;
    void this.attemptReconnect();
  }

  // --- Interne ---------------------------------------------------------------
  private emit(state: LinkState): void {
    this.onStateChange?.(state, this.attempt);
  }

  private clearTimer(): void {
    if (this.timer !== null) {
      clearTimeout(this.timer);
      this.timer = null;
    }
  }

  private async open(deviceId: string, name: string): Promise<string> {
    this.opening = true;
    try {
      await BleClient.connect(deviceId, () => this.handleDisconnect(), { timeout: 12000 });
      this.deviceId = deviceId;
      this.deviceName = name;
      this.decoder.reset();

      await BleClient.startNotifications(deviceId, NUS_SERVICE, NUS_TX, (value) => {
        this.decoder.feed(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
      });

      this.connectedFlag = true;
      this.attempt = 0;
      this.emit('connected');
      this.onConnectionChange?.(true, name);
      return name;
    } finally {
      this.opening = false;
    }
  }

  private async attemptReconnect(): Promise<void> {
    if (!this.wanted || this.connectedFlag || this.opening || !this.deviceId) return;

    this.attempt++;
    this.emit('connecting');
    try {
      await BleClient.initialize({ androidNeverForLocation: true });
      // Après un redémarrage de l'app, le plugin ne connaît plus le boîtier tant
      // qu'on ne le lui a pas fait retrouver par son identifiant (CoreBluetooth
      // `retrievePeripherals` côté iOS). Sans lien BLE, ça échoue sans casse.
      try {
        await BleClient.getDevices([this.deviceId]);
      } catch {
        /* Web Bluetooth ne sait pas reprendre sans geste : l'essai suivant tranchera */
      }
      await this.open(this.deviceId, this.deviceName || this.deviceId);
    } catch {
      this.scheduleReconnect();
    }
  }

  private scheduleReconnect(): void {
    if (!this.wanted || this.timer !== null) return;
    const delay = BACKOFF_MS[Math.min(this.attempt, BACKOFF_MS.length - 1)];
    this.emit('retrying');
    this.timer = setTimeout(() => {
      this.timer = null;
      void this.attemptReconnect();
    }, delay);
  }

  /**
   * iOS suspend l'app en arrière-plan : à son retour, le compte à rebours en
   * cours peut dater de plusieurs minutes. On retente aussitôt plutôt que
   * d'attendre un réveil de `setTimeout` qui n'arrivera pas forcément.
   */
  private installWakeHooks(): void {
    if (this.hooked || typeof document === 'undefined') return;
    this.hooked = true;
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') this.retryNow();
    });
  }

  private handleDisconnect(): void {
    const was = this.connectedFlag;
    this.connectedFlag = false;
    this.decoder.reset();
    if (was) this.onConnectionChange?.(false, '');

    if (this.wanted) {
      // Perte de lien : première relance quasi immédiate, le boîtier est souvent
      // encore là (l'app est simplement repassée au premier plan trop tard).
      this.attempt = 0;
      this.scheduleReconnect();
    } else {
      this.deviceId = '';
      this.emit('off');
    }
  }
}

export const link = new BleLink();
