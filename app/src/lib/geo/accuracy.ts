// -----------------------------------------------------------------------------
// Estimation de la précision d'une position à partir du HDOP et du nombre de
// satellites.
//
// ⚠️ C'est une ESTIMATION, pas une mesure. Le récepteur ne transmet pas d'erreur
// réelle : on dérive un rayon depuis le HDOP (dilution de précision liée à la
// géométrie des satellites) multiplié par une UERE typique de récepteur grand
// public. La valeur affichée doit être lue comme un ordre de grandeur — « il est
// par là, à ~30 m près » — jamais comme une garantie.
// -----------------------------------------------------------------------------

/** Erreur équivalente utilisateur (m) d'un récepteur grand public en conditions correctes. */
const UERE_M = 5;

/** Avec moins de 4 satellites, la puce ne résout pas l'altitude : point 2D, géométrie faible. */
const MIN_SATS_3D = 4;
const PENALTY_2D = 3;

export type Quality = 'good' | 'fair' | 'poor';

export interface Accuracy {
  /** Rayon estimé en mètres. */
  meters: number;
  quality: Quality;
  /** Étiquette courte prête à afficher, ex. « ±12 m ». */
  label: string;
  color: string;
  /** Vrai si le point est un 2D (moins de 4 satellites) : altitude non fiable. */
  is2D: boolean;
  /**
   * Vrai s'il faut dessiner le cercle d'incertitude autour du point.
   *
   * Sur un bon fix, le rayon estimé tient dans le marqueur lui-même : le cercle
   * n'apporterait rien et ajouterait du bruit sur une carte déjà chargée. Il
   * n'apparaît que lorsque l'incertitude devient une information — c'est son
   * apparition, puis sa croissance, qui disent « ce point devient douteux ».
   */
  showCircle: boolean;
}

// Alignés sur la palette de l'interface (app.css) : ces couleurs partent aussi
// dans MapLibre, qui n'hérite pas des variables CSS.
const COLORS: Record<Quality, string> = {
  good: '#4fb85c',
  fair: '#e0a12a',
  poor: '#e0483a',
};

export function estimateAccuracy(sats: number, hdop: number): Accuracy {
  const is2D = sats > 0 && sats < MIN_SATS_3D;

  // HDOP absent (255 côté trame → 0 ici) : on retombe sur le seul nombre de satellites.
  let meters: number;
  if (hdop > 0) {
    meters = hdop * UERE_M;
    if (is2D) meters *= PENALTY_2D;
  } else {
    meters = sats >= 6 ? 15 : 50;
  }
  meters = Math.round(meters);

  let quality: Quality;
  if (is2D || sats < MIN_SATS_3D || meters > 30) quality = 'poor';
  else if (meters > 12 || sats < 6) quality = 'fair';
  else quality = 'good';

  return {
    meters,
    quality,
    label: `±${meters} m`,
    color: COLORS[quality],
    is2D,
    showCircle: quality !== 'good',
  };
}

/**
 * Anneau GeoJSON approximant un cercle de `radiusM` autour d'un point.
 * Approximation équirectangulaire : l'erreur reste très inférieure au rayon
 * lui-même aux échelles qui nous intéressent (quelques dizaines de mètres).
 */
export function circleRing(
  lat: number,
  lon: number,
  radiusM: number,
  steps = 48,
): [number, number][] {
  const dLat = radiusM / 111320;
  const dLon = radiusM / (111320 * Math.cos((lat * Math.PI) / 180) || 1);
  const ring: [number, number][] = [];
  for (let i = 0; i <= steps; i++) {
    const t = (i / steps) * 2 * Math.PI;
    ring.push([lon + dLon * Math.cos(t), lat + dLat * Math.sin(t)]);
  }
  return ring;
}
