import { describe, expect, it } from 'vitest';

import { circleRing, estimateAccuracy } from './accuracy.ts';

describe('estimateAccuracy', () => {
  it('qualifie un bon fix multi-satellites', () => {
    const a = estimateAccuracy(12, 0.6);
    expect(a.meters).toBe(3);
    expect(a.quality).toBe('good');
    expect(a.is2D).toBe(false);
    expect(a.label).toBe('±3 m');
  });

  it('pénalise fortement un point 2D à 3 satellites', () => {
    // Cas réellement observé sur TB-8EB9 : 3 satellites, altitude aberrante.
    const a = estimateAccuracy(3, 2.0);
    expect(a.is2D).toBe(true);
    expect(a.quality).toBe('poor');
    expect(a.meters).toBe(30); // 2.0 × 5 m × 3
  });

  it('reste « poor » à 3 satellites même avec un HDOP flatteur', () => {
    expect(estimateAccuracy(3, 0.5).quality).toBe('poor');
  });

  it('dégrade la qualité quand le HDOP monte', () => {
    expect(estimateAccuracy(8, 1.0).quality).toBe('good');
    expect(estimateAccuracy(8, 3.0).quality).toBe('fair');
    expect(estimateAccuracy(8, 8.0).quality).toBe('poor');
  });

  it('retombe sur le nombre de satellites quand le HDOP est inconnu', () => {
    expect(estimateAccuracy(8, 0).meters).toBe(15);
    expect(estimateAccuracy(4, 0).meters).toBe(50);
    expect(estimateAccuracy(4, 0).quality).toBe('poor');
  });

  it('ne réclame un cercle que lorsque l’incertitude dit quelque chose', () => {
    // Beaucoup de satellites, bonne géométrie : le rayon tient dans le marqueur.
    expect(estimateAccuracy(12, 0.6).showCircle).toBe(false);
    expect(estimateAccuracy(8, 1.0).showCircle).toBe(false);
    // Dès que ça se dégrade, le cercle apparaît et grandit avec le doute.
    expect(estimateAccuracy(8, 3.0).showCircle).toBe(true);
    expect(estimateAccuracy(3, 2.0).showCircle).toBe(true);
    expect(estimateAccuracy(8, 8.0).meters).toBeGreaterThan(estimateAccuracy(8, 3.0).meters);
  });
});

describe('circleRing', () => {
  it('produit un anneau fermé', () => {
    const ring = circleRing(48.8566, 2.3522, 50, 16);
    expect(ring).toHaveLength(17);
    expect(ring[0][0]).toBeCloseTo(ring[16][0], 10);
    expect(ring[0][1]).toBeCloseTo(ring[16][1], 10);
  });

  it('respecte le rayon demandé en latitude', () => {
    const ring = circleRing(48.8566, 2.3522, 100, 4);
    // Le point à 90° est à 100 m au nord : ~0,000898° de latitude.
    const north = ring[1];
    expect((north[1] - 48.8566) * 111320).toBeCloseTo(100, 0);
  });

  it('élargit le pas de longitude avec la latitude', () => {
    const equator = circleRing(0, 0, 100, 4);
    const north = circleRing(60, 0, 100, 4);
    // À 60° de latitude, un degré de longitude vaut deux fois moins de mètres.
    expect(Math.abs(north[0][0])).toBeCloseTo(Math.abs(equator[0][0]) * 2, 4);
  });
});
