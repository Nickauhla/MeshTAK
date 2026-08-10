<script lang="ts">
  import { session } from '../state/session.svelte.ts';

  let busy = $state(false);

  async function connect() {
    busy = true;
    await session.connect();
    busy = false;
  }
</script>

<div class="connect">
  <div class="frame">
    <!-- Réticule : les arcs tournent pendant la recherche du boîtier. -->
    <svg class="reticle" class:scanning={busy} viewBox="0 0 120 120" aria-hidden="true">
      <circle class="ring" cx="60" cy="60" r="52" />
      <g class="sweep">
        <path class="arc" d="M60 4a56 56 0 0126 6.6" />
        <path class="arc" d="M60 116a56 56 0 01-26-6.6" />
      </g>
      <circle class="ring inner" cx="60" cy="60" r="30" />
      <path class="cross" d="M60 22v14M60 84v14M22 60h14M84 60h14" />
      <circle class="core" cx="60" cy="60" r="7" />
    </svg>

    <h1>MeshRadio</h1>
    <p class="sub">Réseau tactique LoRa 868 MHz — hors réseau cellulaire</p>

    <button class="btn-primary go" onclick={connect} disabled={busy}>
      {busy ? 'Recherche…' : 'Connecter le T-Beam'}
    </button>

    {#if session.lastError}
      <p class="err"><span class="tag">échec</span>{session.lastError}</p>
    {/if}
  </div>

  <ol class="steps">
    <li><span class="n mono">01</span>Allumez le T-Beam (LED d'alimentation).</li>
    <li><span class="n mono">02</span>Activez le Bluetooth du téléphone.</li>
    <li>
      <span class="n mono">03</span>Sélectionnez <code class="mono">MeshRadio-XXXX</code> dans la liste.
    </li>
  </ol>
</div>

<style>
  .connect {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 32px;
    padding: 32px 26px calc(32px + env(safe-area-inset-bottom));
    text-align: center;
  }
  .frame {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 12px;
  }

  .reticle {
    width: 108px;
    height: 108px;
    margin-bottom: 6px;
  }
  .ring {
    fill: none;
    stroke: var(--line);
    stroke-width: 1;
  }
  .ring.inner {
    stroke-dasharray: 3 7;
  }
  .cross {
    stroke: var(--accent);
    stroke-width: 1.5;
    opacity: 0.65;
  }
  .arc {
    fill: none;
    stroke: var(--accent);
    stroke-width: 2;
    stroke-linecap: round;
  }
  .core {
    fill: var(--accent);
    filter: drop-shadow(0 0 8px rgba(155, 217, 60, 0.8));
  }
  .sweep {
    transform-origin: 60px 60px;
  }
  .scanning .sweep {
    animation: spin 1.6s linear infinite;
  }
  .scanning .core {
    animation: blink 1s ease-in-out infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
  @keyframes blink {
    50% {
      opacity: 0.25;
    }
  }

  h1 {
    margin: 0;
    font-size: 30px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.26em;
    /* La lettre est espacée à droite : on recentre optiquement. */
    text-indent: 0.26em;
  }
  .sub {
    color: var(--muted);
    font-size: 13px;
    letter-spacing: 0.04em;
    margin: 0 0 18px;
    max-width: 260px;
    line-height: 1.55;
  }
  .go {
    padding: 15px 30px;
    font-size: 15px;
    letter-spacing: 0.14em;
    min-width: 250px;
  }
  .err {
    color: var(--danger);
    font-size: 12px;
    max-width: 300px;
    line-height: 1.5;
    margin: 4px 0 0;
  }
  .tag {
    display: inline-block;
    border: 1px solid rgba(224, 72, 58, 0.5);
    text-transform: uppercase;
    letter-spacing: 0.14em;
    font-size: 9px;
    padding: 1px 5px;
    margin-right: 7px;
    vertical-align: 1px;
  }

  /* Procédure : numérotée comme une check-list d'équipement. */
  .steps {
    list-style: none;
    text-align: left;
    color: var(--muted);
    font-size: 13px;
    margin: 0;
    padding: 14px 0 0;
    max-width: 300px;
    border-top: 1px solid var(--line-soft);
    display: flex;
    flex-direction: column;
    gap: 9px;
  }
  .steps li {
    display: flex;
    gap: 10px;
    line-height: 1.4;
  }
  .n {
    color: var(--accent);
    font-size: 11px;
    letter-spacing: 0.08em;
    padding-top: 2px;
    flex: none;
  }
  code {
    color: var(--fg);
    background: var(--surface-2);
    border: 1px solid var(--line);
    padding: 0 4px;
    font-size: 12px;
  }
</style>
