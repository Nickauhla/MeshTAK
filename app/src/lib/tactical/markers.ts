// -----------------------------------------------------------------------------
// Catalogue des points tactiques — nomenclature ATAK / MIL-STD-2525.
//
// Le protocole ne transporte qu'un octet (`kind`, cf. PROTOCOL.md §4.7) : tout ce
// qui est ici — cadre, couleur, libellé, durée de validité — est de la
// PRÉSENTATION, et peut changer sans toucher à la radio.
//
// Les quatre premiers cadres sont ceux d'ATAK et se lisent sans légende par
// quiconque a déjà utilisé un TAK : losange = hostile, rectangle = ami,
// quatrefeuille = incertain. La FORME porte l'information autant que la couleur —
// sous soleil rasant ou sur fond de forêt, la couleur seule ne suffit pas.
// -----------------------------------------------------------------------------
import { MarkerKind } from '../proto/frames.ts';

export interface MarkerSpec {
  kind: number;
  /** Intitulé du menu circulaire. */
  label: string;
  /** Étiquette courte posée sur la carte. */
  short: string;
  color: string;
  /** Type Cursor-on-Target correspondant, pour une future passerelle ATAK. */
  cot: string;
  /**
   * Validité par défaut, en minutes. Un contact périme — la position d'un ennemi
   * vue il y a un quart d'heure est un mensonge. Un élément de terrain, non.
   */
  ttlMin: number;
  /** Contenu SVG dans un carré de 32×32. */
  glyph: string;
}

const HOSTILE_FRAME = '<path class="frame" d="M16 3 L29 16 L16 29 L3 16 Z"/>';
const FRIEND_FRAME = '<rect class="frame" x="4" y="8" width="24" height="16" rx="2"/>';
// Quatrefeuille du cadre « inconnu » d'ATAK, approchée en courbes quadratiques.
const UNKNOWN_FRAME =
  '<path class="frame" d="M16 4 Q23 4 23 11 Q29 11 29 16 Q29 22 23 22 Q23 28 16 28' +
  ' Q9 28 9 22 Q3 22 3 16 Q3 11 9 11 Q9 4 16 4 Z"/>';
const NEUTRAL_FRAME = '<circle class="frame" cx="16" cy="16" r="12"/>';

export const MARKER_SPECS: MarkerSpec[] = [
  {
    kind: MarkerKind.HOSTILE,
    label: 'Ennemi',
    short: 'ENN',
    color: '#e0483a',
    cot: 'a-h-G',
    ttlMin: 10,
    glyph: HOSTILE_FRAME + '<path class="ink" d="M16 10v8M16 21.5v.5"/>',
  },
  {
    kind: MarkerKind.HOSTILE_VEHICLE,
    label: 'Véhicule ennemi',
    short: 'VÉH',
    color: '#e0483a',
    cot: 'a-h-G-E-V',
    ttlMin: 10,
    glyph:
      HOSTILE_FRAME +
      '<path class="ink" d="M9 18h14M10 18v-3h5l3 3"/>' +
      '<circle class="ink" cx="12" cy="20" r="1.6"/><circle class="ink" cx="20" cy="20" r="1.6"/>',
  },
  {
    kind: MarkerKind.FRIEND,
    label: 'Ami',
    short: 'AMI',
    color: '#4fc3e8',
    cot: 'a-f-G',
    ttlMin: 10,
    glyph: FRIEND_FRAME + '<path class="ink" d="M8 12l8 8 8-8"/>',
  },
  {
    kind: MarkerKind.UNKNOWN,
    label: 'Incertain',
    short: '?',
    color: '#e0a12a',
    cot: 'a-u-G',
    ttlMin: 10,
    glyph: UNKNOWN_FRAME + '<path class="ink" d="M13 13a3 3 0 116 0c0 2-3 2.2-3 4.5M16 21.5v.5"/>',
  },
  {
    kind: MarkerKind.OBJECTIVE,
    label: 'Objectif',
    short: 'OBJ',
    color: '#9bd93c',
    cot: 'b-m-p-w',
    ttlMin: 0,
    glyph: '<path class="ink flag" d="M9 28V4l14 5-14 5"/>',
  },
  {
    kind: MarkerKind.VIP,
    label: 'VIP',
    short: 'VIP',
    color: '#a97cf0',
    cot: 'a-n-G',
    ttlMin: 0,
    glyph: NEUTRAL_FRAME + '<path class="ink fill" d="M16 8l2.4 5 5.6.8-4 3.9 1 5.5-5-2.6-5 2.6 1-5.5-4-3.9 5.6-.8z"/>',
  },
  {
    kind: MarkerKind.HAZARD,
    label: 'Danger',
    short: 'DGR',
    color: '#f0761f',
    cot: 'b-m-p-s-m',
    ttlMin: 0,
    glyph: '<path class="frame" d="M16 4 L29 27 H3 Z"/><path class="ink" d="M16 12v8M16 23.5v.5"/>',
  },
  {
    kind: MarkerKind.RALLY,
    label: 'Rassemblement',
    short: 'RAS',
    color: '#57d9c0',
    cot: 'b-m-p-s-m',
    ttlMin: 0,
    glyph:
      NEUTRAL_FRAME +
      '<path class="ink" d="M16 6v5M16 26v-5M6 16h5M26 16h-5"/>' +
      '<circle class="ink fill" cx="16" cy="16" r="3"/>',
  },
];

const BY_KIND = new Map(MARKER_SPECS.map((s) => [s.kind, s]));

export function markerSpec(kind: number): MarkerSpec | null {
  return BY_KIND.get(kind) ?? null;
}

/**
 * Icône complète, prête à insérer dans le DOM. Sert au menu circulaire (Svelte)
 * comme aux marqueurs MapLibre (éléments HTML créés à la main) : une seule
 * définition pour les deux, sinon les deux dérivent.
 */
export function markerSvg(kind: number, size = 26): string {
  const spec = markerSpec(kind);
  if (!spec) return '';
  return (
    `<svg class="mk-glyph" viewBox="0 0 32 32" width="${size}" height="${size}"` +
    ` style="--c:${spec.color}" aria-hidden="true">${spec.glyph}</svg>`
  );
}
