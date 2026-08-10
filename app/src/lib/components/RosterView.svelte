<script lang="ts">
  import { estimateAccuracy } from '../geo/accuracy.ts';
  import { PlayerStatus } from '../proto/frames.ts';
  import { session } from '../state/session.svelte.ts';

  const STATUS = [
    { v: PlayerStatus.OK, label: 'Opérationnel', short: 'OP', color: 'var(--ok)' },
    { v: PlayerStatus.HIT, label: 'Touché', short: 'TOUCHÉ', color: 'var(--warn)' },
    { v: PlayerStatus.ELIMINATED, label: 'Éliminé', short: 'ELIM', color: 'var(--danger)' },
    { v: PlayerStatus.NEED_HELP, label: 'Demande de l’aide', short: 'AIDE', color: 'var(--help)' },
  ];
  const ROLE = ['Joueur', 'Chef', 'Relais', 'Commandement'];

  let myStatus = $state<number>(PlayerStatus.OK);
  let now = $state(Date.now());

  $effect(() => {
    const t = setInterval(() => (now = Date.now()), 1000);
    return () => clearInterval(t);
  });

  function age(ts: number, nowMs: number): string {
    if (!ts) return '–';
    const s = Math.round((nowMs - ts) / 1000);
    if (s < 60) return `${s} s`;
    if (s < 3600) return `${Math.round(s / 60)} min`;
    return `${Math.round(s / 3600)} h`;
  }

  /** Un contact vieux n'est plus une position : la couleur le dit avant le texte. */
  function ageColor(ts: number, nowMs: number): string {
    if (!ts) return 'var(--dim)';
    const s = (nowMs - ts) / 1000;
    if (s < 45) return 'var(--muted)';
    if (s < 180) return 'var(--warn)';
    return 'var(--danger)';
  }

  function distanceM(lat: number, lon: number): string {
    const me = session.myPosition;
    if (!me) return '–';
    const R = 6371000;
    const dLat = ((lat - me.lat) * Math.PI) / 180;
    const dLon = ((lon - me.lon) * Math.PI) / 180;
    const a =
      Math.sin(dLat / 2) ** 2 +
      Math.cos((me.lat * Math.PI) / 180) * Math.cos((lat * Math.PI) / 180) * Math.sin(dLon / 2) ** 2;
    const d = 2 * R * Math.asin(Math.sqrt(a));
    return d < 1000 ? `${Math.round(d)} m` : `${(d / 1000).toFixed(1)} km`;
  }

  async function declare(v: number) {
    myStatus = v;
    await session.setPlayerStatus(v);
  }
</script>

<div class="panel">
  <section>
    <h2 class="hud-title">Mon statut</h2>
    <div class="status-row">
      {#each STATUS as s (s.v)}
        <button
          class:selected={myStatus === s.v}
          style:--c={s.color}
          onclick={() => declare(s.v)}
          disabled={!session.connected}
        >
          <span class="bar"></span>
          {s.label}
        </button>
      {/each}
    </div>
  </section>

  <section>
    <h2 class="hud-title">
      {session.config.squadName}
      <span class="count mono">{session.squadMates.length} contact(s)</span>
    </h2>

    {#if session.squadMates.length === 0}
      <p class="empty">
        Aucun coéquipier entendu. Les autres joueurs doivent avoir été validés dans cette escouade —
        un boîtier réglé sur le même nom mais sans la clé reste muet pour vous.
      </p>
    {:else}
      <ul class="roster">
        {#each session.squadMates as node (node.addr)}
          {@const s = STATUS[node.status] ?? STATUS[0]}
          {@const acc = estimateAccuracy(node.sats, node.hdop)}
          <li
            style:--c={s.color}
            class:uncertain={node.lat !== null && acc.quality === 'poor'}
          >
            <div class="who">
              <div class="line1">
                <strong>{node.callsign}</strong>
                <span class="addr mono">#{node.addr}</span>
              </div>
              <small>
                <span class="state">{s.short}</span>
                <span class="sep">·</span>{ROLE[node.role] ?? 'Joueur'}
                {#if node.viaRelay}<span class="sep">·</span><span class="relay">relayé</span>{/if}
              </small>
            </div>
            <div class="metrics">
              <span class="dist mono">
                {node.lat !== null && node.lon !== null ? distanceM(node.lat, node.lon) : '–'}
              </span>
              <small class="mono">
                {#if node.lat !== null}<em style:color={acc.color}>{acc.label}</em
                  ><span class="sep">·</span>{/if}
                {node.battPct === 255 ? '–' : node.battPct + '%'}<span class="sep">·</span>
                <span style:color={ageColor(node.lastHeard, now)}>{age(node.lastHeard, now)}</span>
                {#if node.lat !== null && acc.is2D}<span class="sep">·</span><span class="warn"
                    >2D</span
                  >{/if}
              </small>
            </div>
          </li>
        {/each}
      </ul>
    {/if}
  </section>

  {#if session.status && session.status.peerCount > session.squadMates.length}
    <section>
      <h2 class="hud-title">Autour de vous</h2>
      <p class="empty">
        <b class="mono">{session.status.peerCount}</b> boîtier(s) entendu(s) au total, dont
        <b class="mono">{session.status.peerCount - session.squadMates.length}</b> hors de votre
        escouade. Leurs échanges vous sont illisibles, mais ils <b>relaient</b> les vôtres.
      </p>
    </section>
  {/if}
</div>

<style>
  .panel {
    flex: 1;
    overflow-y: auto;
    padding: 16px 14px calc(16px + env(safe-area-inset-bottom));
    display: flex;
    flex-direction: column;
    gap: 26px;
  }
  .count {
    font-size: 11px;
    letter-spacing: 0.04em;
    color: var(--dim);
    text-transform: none;
    flex: none;
    order: 9; /* toujours en bout de ligne, après le filet */
  }

  /* Déclaration de statut : quatre plaques, chacune barrée de sa couleur. */
  .status-row {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 7px;
  }
  .status-row button {
    position: relative;
    padding: 13px 10px 13px 16px;
    background: var(--surface);
    border-color: var(--line);
    color: var(--muted);
    font-size: 12px;
    letter-spacing: 0.1em;
    text-align: left;
    overflow: hidden;
  }
  .status-row .bar {
    position: absolute;
    left: 0;
    top: 0;
    bottom: 0;
    width: 3px;
    background: var(--c);
    opacity: 0.4;
  }
  .status-row button.selected {
    border-color: var(--c);
    color: var(--c);
    background: var(--surface-2);
    box-shadow: inset 0 0 26px -14px var(--c);
  }
  .status-row button.selected .bar {
    opacity: 1;
    width: 4px;
    box-shadow: 0 0 10px var(--c);
  }

  /* Fiche de contact : barre de statut à gauche, mesures alignées à droite. */
  .roster {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .roster li {
    display: flex;
    align-items: center;
    gap: 12px;
    background: var(--surface);
    border: 1px solid var(--line-soft);
    border-left: 3px solid var(--c);
    padding: 10px 12px;
  }
  /* Position douteuse : le cadre s'ouvre, la fiche s'efface légèrement. */
  .roster li.uncertain {
    border-left-style: dashed;
    opacity: 0.72;
  }
  .who {
    flex: 1;
    min-width: 0;
  }
  .line1 {
    display: flex;
    align-items: baseline;
    gap: 7px;
  }
  .who strong {
    font-size: 15px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .addr {
    font-size: 11px;
    color: var(--dim);
  }
  .state {
    color: var(--c);
    font-weight: 600;
    letter-spacing: 0.12em;
  }
  .relay {
    color: var(--self);
  }
  .metrics {
    text-align: right;
    display: flex;
    flex-direction: column;
    flex: none;
  }
  .dist {
    font-size: 16px;
    font-weight: 500;
    line-height: 1.15;
  }
  small {
    display: block;
    color: var(--muted);
    font-size: 11px;
  }
  .sep {
    color: var(--dim);
    margin: 0 4px;
  }
  em {
    font-style: normal;
  }
  .warn {
    color: var(--warn);
  }
  .empty {
    color: var(--muted);
    font-size: 12.5px;
    line-height: 1.6;
    margin: 0;
    border-left: 2px solid var(--line);
    padding-left: 11px;
  }
  b {
    color: var(--fg);
  }
</style>
