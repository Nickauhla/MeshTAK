<script lang="ts">
  // ---------------------------------------------------------------------------
  // Menu circulaire de pose de point tactique, ouvert par appui long sur la carte.
  //
  // La disposition en couronne n'est pas décorative : le doigt est déjà à
  // l'endroit visé, chaque choix est à la même distance de lui, et la carte reste
  // visible entre les plaques. Un menu déroulant obligerait à quitter le point
  // des yeux et à traverser l'écran.
  // ---------------------------------------------------------------------------
  import { MARKER_SPECS, markerSvg } from '../tactical/markers.ts';

  interface Props {
    /** Position de l'appui, en pixels dans le cadre de la carte. */
    x: number;
    y: number;
    onpick: (kind: number) => void;
    onclose: () => void;
  }
  let { x, y, onpick, onclose }: Props = $props();

  const RADIUS = 84;
  const SLOT = 54;

  let w = $state(0);
  let h = $state(0);

  // Le menu s'ouvre pendant que le doigt est encore posé : il ne prend ses
  // ordres qu'une fois la couronne déployée, sinon le relâchement de l'appui
  // long refermerait le menu qu'il vient d'ouvrir.
  let armed = $state(false);
  $effect(() => {
    const t = setTimeout(() => (armed = true), 200);
    return () => clearTimeout(t);
  });

  // La couronne reste entière à l'écran même si l'appui a lieu dans un coin.
  const half = RADIUS + SLOT / 2 + 6;
  const cx = $derived(Math.min(Math.max(x, half), Math.max(half, w - half)));
  const cy = $derived(Math.min(Math.max(y, half), Math.max(half, h - half)));

  const slots = MARKER_SPECS.map((spec, i) => {
    const a = (i / MARKER_SPECS.length) * 2 * Math.PI - Math.PI / 2;
    return {
      spec,
      dx: Math.round(Math.cos(a) * RADIUS),
      dy: Math.round(Math.sin(a) * RADIUS),
      delay: i * 14,
    };
  });

  function stop(e: Event) {
    e.stopPropagation();
  }
</script>

<!-- Le voile capte tout : un appui à côté referme, et la carte ne bouge pas
     pendant que le menu est ouvert. -->
<div
  class="backdrop"
  bind:clientWidth={w}
  bind:clientHeight={h}
  onpointerdown={(e) => {
    stop(e);
    if (armed) onclose();
  }}
  oncontextmenu={(e) => e.preventDefault()}
  role="presentation"
>
  <div class="wheel" style="left:{cx}px; top:{cy}px">
    <span class="pin" style="left:{x - cx}px; top:{y - cy}px"></span>

    {#each slots as s (s.spec.kind)}
      <button
        class="slot"
        style="--dx:{s.dx}px; --dy:{s.dy}px; --c:{s.spec.color}; --d:{s.delay}ms"
        title={s.spec.label}
        onpointerdown={stop}
        onclick={(e) => {
          stop(e);
          if (armed) onpick(s.spec.kind);
        }}
      >
        {@html markerSvg(s.spec.kind, 24)}
        <span class="tag">{s.spec.short}</span>
      </button>
    {/each}

    <button
      class="cancel"
      onpointerdown={stop}
      onclick={(e) => {
        stop(e);
        if (armed) onclose();
      }}
    >
      ✕
    </button>
  </div>
</div>

<style>
  .backdrop {
    position: absolute;
    inset: 0;
    z-index: 20;
    background: radial-gradient(circle at center, rgba(8, 11, 8, 0.2), rgba(8, 11, 8, 0.55));
    touch-action: none;
  }

  .wheel {
    position: absolute;
    width: 0;
    height: 0;
  }

  /* Rappel du point exactement visé : la couronne peut être décalée pour tenir à
     l'écran, le point posé sera là, pas au centre du menu. */
  .pin {
    position: absolute;
    width: 13px;
    height: 13px;
    margin: -7px 0 0 -7px;
    border: 1px solid var(--accent);
    border-radius: 50%;
    box-shadow: 0 0 10px -2px var(--accent);
  }
  .pin::after {
    content: '';
    position: absolute;
    inset: 5px;
    background: var(--accent);
    border-radius: 50%;
  }

  .slot,
  .cancel {
    position: absolute;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 1px;
    border-radius: 50%;
    padding: 0;
    background: rgba(8, 11, 8, 0.94);
    backdrop-filter: blur(2px);
  }

  .slot {
    width: 54px;
    height: 54px;
    margin: -27px 0 0 -27px;
    border: 1px solid var(--c);
    color: var(--c);
    box-shadow:
      0 0 0 1px rgba(0, 0, 0, 0.6),
      0 0 16px -6px var(--c);
    transform: translate(var(--dx), var(--dy));
    animation: deploy 130ms ease-out var(--d) backwards;
  }
  .slot:active {
    background: var(--c);
    color: #080b08;
    transform: translate(var(--dx), var(--dy)) scale(0.94);
  }
  .slot .tag {
    font-size: 8px;
    font-weight: 700;
    letter-spacing: 0.1em;
    opacity: 0.85;
  }

  /* Les plaques jaillissent du point visé : le geste et l'animation racontent la
     même chose. */
  @keyframes deploy {
    from {
      transform: translate(0, 0) scale(0.4);
      opacity: 0;
    }
  }

  .cancel {
    width: 40px;
    height: 40px;
    margin: -20px 0 0 -20px;
    border: 1px solid var(--line);
    color: var(--muted);
    font-size: 15px;
    opacity: 0;
    animation: reveal 160ms ease-out 140ms forwards;
  }
  @keyframes reveal {
    to {
      opacity: 1;
    }
  }
</style>
