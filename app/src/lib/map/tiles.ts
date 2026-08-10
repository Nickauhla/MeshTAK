// -----------------------------------------------------------------------------
// Source de fond de carte. Trois modes, choisis dans les réglages :
//
//   osm      : tuiles OpenStreetMap en ligne — pratique en développement.
//   pmtiles  : archive PMTiles locale (mono-fichier) — LE mode terrain, hors réseau.
//   blank    : fond uni + grille — aucun réseau, utile pour tester la radio seule.
//
// Pour préparer une carte hors-ligne : découper la zone de jeu en tuiles raster
// puis convertir en .pmtiles (`pmtiles convert zone.mbtiles zone.pmtiles`), et
// déposer le fichier dans app/public/maps/.
// -----------------------------------------------------------------------------
import maplibregl, { type StyleSpecification } from 'maplibre-gl';
import { Protocol } from 'pmtiles';

export type TileMode = 'osm' | 'pmtiles' | 'blank';

export interface TileSettings {
  mode: TileMode;
  pmtilesUrl: string;
}

const KEY = 'meshradio.tiles';
const DEFAULTS: TileSettings = { mode: 'osm', pmtilesUrl: '/maps/zone.pmtiles' };

export function loadTileSettings(): TileSettings {
  try {
    return { ...DEFAULTS, ...JSON.parse(localStorage.getItem(KEY) ?? '{}') };
  } catch {
    return { ...DEFAULTS };
  }
}

export function saveTileSettings(s: TileSettings): void {
  localStorage.setItem(KEY, JSON.stringify(s));
}

let pmtilesRegistered = false;
export function registerPmtiles(): void {
  if (pmtilesRegistered) return;
  const protocol = new Protocol();
  maplibregl.addProtocol('pmtiles', protocol.tile);
  pmtilesRegistered = true;
}

const background = {
  id: 'fond',
  type: 'background' as const,
  paint: { 'background-color': '#080b08' },
};

export function buildStyle(s: TileSettings): StyleSpecification {
  if (s.mode === 'osm') {
    return {
      version: 8,
      sources: {
        osm: {
          type: 'raster',
          tiles: ['https://tile.openstreetmap.org/{z}/{x}/{y}.png'],
          tileSize: 256,
          maxzoom: 19,
          attribution: '© OpenStreetMap',
        },
      },
      layers: [background, { id: 'osm', type: 'raster', source: 'osm' }],
    };
  }

  if (s.mode === 'pmtiles') {
    registerPmtiles();
    return {
      version: 8,
      sources: {
        zone: { type: 'raster', url: `pmtiles://${s.pmtilesUrl}`, tileSize: 256 },
      },
      layers: [background, { id: 'zone', type: 'raster', source: 'zone' }],
    };
  }

  // Mode « aucun » : fond laissé transparent pour que la graticule de l'interface
  // (app.css) serve de repère d'échelle, au lieu d'un aplat noir muet.
  return {
    version: 8,
    sources: {},
    layers: [{ ...background, paint: { ...background.paint, 'background-opacity': 0 } }],
  };
}
