<script lang="ts">
  import { loadTileSettings, saveTileSettings, type TileMode } from '../map/tiles.ts';
  import { SquadState } from '../proto/frames.ts';
  import { session } from '../state/session.svelte.ts';

  let callsign = $state(session.config.callsign);
  let tiles = $state(loadTileSettings());
  let advanced = $state(false);
  let confirmLeave = $state(false);

  let sf = $state(session.config.sf);
  let txPower = $state(session.config.txPower);
  let posIntervalS = $state(session.config.posIntervalS);
  let dutyPercent = $state(session.config.dutyPercent);

  // Recharge les champs quand le device renvoie sa configuration.
  $effect(() => {
    const c = session.config;
    callsign = c.callsign;
    sf = c.sf;
    txPower = c.txPower;
    posIntervalS = c.posIntervalS;
    dutyPercent = c.dutyPercent;
  });

  async function apply() {
    await session.pushConfig({
      ...session.config,
      callsign,
      sf,
      txPower,
      posIntervalS,
      dutyPercent,
    });
  }

  function applyTiles(mode: TileMode) {
    tiles = { ...tiles, mode };
    saveTileSettings(tiles);
  }
</script>

<div class="panel">
  <section>
    <h2 class="hud-title">Escouade</h2>
    <dl>
      <dt>Nom</dt>
      <dd>{session.config.squadName}</dd>
      <dt>Mon numéro</dt>
      <dd class="mono">#{session.config.addr}</dd>
      <dt>Rôle</dt>
      <dd>{session.config.state === SquadState.LEADER ? 'Chef d’escouade' : 'Membre'}</dd>
    </dl>
    <p class="hint">
      Les échanges de l'escouade sont chiffrés avec une clé que seul votre boîtier détient. Elle ne
      transite jamais par le téléphone.
    </p>
    {#if confirmLeave}
      <div class="row">
        <button class="btn-danger grow" onclick={() => session.leaveSquad()}>
          Confirmer — quitter {session.config.squadName}
        </button>
        <button class="btn-ghost" onclick={() => (confirmLeave = false)}>Annuler</button>
      </div>
      <p class="hint warn">
        {session.config.state === SquadState.LEADER
          ? 'Vous en êtes le chef : plus personne ne pourra être validé, et les membres actuels resteront entre eux.'
          : 'Il faudra une nouvelle validation du chef pour revenir.'}
      </p>
    {:else}
      <button class="btn-danger" onclick={() => (confirmLeave = true)}>Quitter l'escouade</button>
    {/if}
  </section>

  <section>
    <h2 class="hud-title">Identité</h2>
    <div class="row">
      <input bind:value={callsign} maxlength="16" placeholder="Indicatif" />
      <button class="btn-primary" onclick={apply} disabled={!session.connected}>Appliquer</button>
    </div>
    <dl>
      <dt>Module BLE</dt>
      <dd class="mono">{session.deviceName || '–'}</dd>
      <dt>Empreinte</dt>
      <dd class="mono fp">{session.deviceFingerprint || '–'}</dd>
    </dl>
    <p class="hint">
      C'est ce que le chef lit sur son téléphone au moment de vous valider. Annoncez-la de vive voix :
      c'est le seul moyen pour lui de savoir qu'il accepte bien vous.
    </p>
  </section>

  <section>
    <h2 class="hud-title">Fond de carte</h2>
    <div class="row modes">
      <button class:btn-selected={tiles.mode === 'osm'} onclick={() => applyTiles('osm')}>
        OSM<small>en ligne</small>
      </button>
      <button class:btn-selected={tiles.mode === 'pmtiles'} onclick={() => applyTiles('pmtiles')}>
        PMTiles<small>hors-ligne</small>
      </button>
      <button class:btn-selected={tiles.mode === 'blank'} onclick={() => applyTiles('blank')}>
        Aucun<small>grille</small>
      </button>
    </div>
    <div class="row">
      <input
        class="mono"
        bind:value={tiles.pmtilesUrl}
        placeholder="/maps/zone.pmtiles"
        onblur={() => saveTileSettings(tiles)}
      />
    </div>
    <p class="hint">Le changement de fond prend effet au prochain affichage de la carte.</p>
  </section>

  <section>
    <h2 class="hud-title">
      Radio
      <button class="link" onclick={() => (advanced = !advanced)}>
        {advanced ? '— masquer' : '+ afficher'}
      </button>
    </h2>

    {#if advanced}
      <label>
        <span class="k">Spreading factor<b class="mono">SF{sf}</b></span>
        <input type="range" min="5" max="12" bind:value={sf} />
        <small>SF bas = rapide et court ; SF haut = lent et longue portée.</small>
      </label>
      <label>
        <span class="k">Puissance<b class="mono">{txPower} dBm</b></span>
        <input type="range" min="-9" max="22" bind:value={txPower} />
      </label>
      <label>
        <span class="k">Intervalle de position<b class="mono">{posIntervalS} s</b></span>
        <input type="range" min="2" max="60" bind:value={posIntervalS} />
      </label>
      <label>
        <span class="k">Budget de temps d'antenne<b class="mono">{dutyPercent} %</b></span>
        <input type="range" min="1" max="20" bind:value={dutyPercent} />
        <small>
          La sous-bande 869,4–869,65 MHz autorise 10 % en Europe. Au-delà, vous sortez de la
          réglementation.
        </small>
      </label>
      <button class="btn-primary full" onclick={apply} disabled={!session.connected}>
        Appliquer sur le T-Beam
      </button>
      <p class="hint mono">
        {(session.config.freqHz / 1e6).toFixed(3)} MHz · BW 250 kHz · CR 4/5
      </p>
    {/if}
  </section>

  {#if session.status}
    <section>
      <h2 class="hud-title">Diagnostic device</h2>
      <dl>
        <dt>GPS</dt>
        <dd class="mono">
          {session.status.fixValid ? 'fix' : 'pas de fix'} · {session.status.sats} sat · HDOP {session
            .status.hdop || '–'}
        </dd>
        <dt>Batterie</dt>
        <dd class="mono">
          {session.status.battPct === 255 ? '–' : session.status.battPct + ' %'} ·
          {session.status.battMv} mV
          {session.status.charging ? '· en charge' : ''}
        </dd>
        <dt>Temps d'antenne</dt>
        <dd class="mono">{session.status.airtimePercent} % sur la dernière minute</dd>
        <dt>Dernier signal</dt>
        <dd class="mono">{session.status.lastRssi} dBm · SNR {session.status.lastSnr} dB</dd>
        <dt>Boîtiers entendus</dt>
        <dd class="mono">{session.status.peerCount}</dd>
      </dl>
    </section>
  {/if}

  <section>
    <button class="btn-ghost full" onclick={() => session.disconnect()} disabled={!session.connected}>
      Déconnecter le T-Beam
    </button>
  </section>
</div>

<style>
  .panel {
    flex: 1;
    overflow-y: auto;
    padding: 16px 14px calc(20px + env(safe-area-inset-bottom));
    display: flex;
    flex-direction: column;
    gap: 26px;
  }
  .row {
    display: flex;
    gap: 7px;
    margin-bottom: 8px;
  }
  .grow {
    flex: 1;
  }
  .full {
    width: 100%;
  }

  /* Fond de carte : trois plaques d'égale largeur, intitulé + précision dessous. */
  .row.modes button {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    font-size: 11px;
    padding: 9px 4px;
    color: var(--muted);
  }
  .row.modes small {
    font-size: 9px;
    letter-spacing: 0.1em;
    color: var(--dim);
    text-transform: uppercase;
  }
  .row.modes button.btn-selected small {
    color: inherit;
    opacity: 0.75;
  }

  button.link {
    background: none;
    border: none;
    color: var(--accent);
    padding: 0;
    font-size: 10px;
    letter-spacing: 0.14em;
    order: 9;
    flex: none;
  }

  /* Curseurs : intitulé à gauche, valeur courante en chasse fixe à droite. */
  label {
    display: flex;
    flex-direction: column;
    gap: 7px;
    font-size: 13px;
    margin-bottom: 15px;
  }
  .k {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    color: var(--muted);
    text-transform: uppercase;
    font-size: 11px;
    letter-spacing: 0.1em;
  }
  /* La valeur garde sa casse : « dBm » n'est pas « DBM ». */
  .k b {
    color: var(--accent);
    font-size: 13px;
    letter-spacing: 0;
    text-transform: none;
  }
  input[type='range'] {
    height: 22px;
  }

  small,
  .hint {
    color: var(--muted);
    font-size: 11.5px;
    line-height: 1.55;
  }
  .hint {
    margin: 6px 0;
    border-left: 2px solid var(--line);
    padding-left: 10px;
  }
  .hint.warn {
    color: var(--warn);
    border-left-color: rgba(224, 161, 42, 0.5);
  }

  /* Relevés : deux colonnes, libellé effacé, valeur en avant. */
  dl {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 5px 14px;
    margin: 0 0 10px;
    font-size: 13px;
  }
  dt {
    color: var(--dim);
    text-transform: uppercase;
    font-size: 10px;
    letter-spacing: 0.12em;
    padding-top: 3px;
  }
  dd {
    margin: 0;
    color: var(--fg);
  }
  /* L'empreinte se lit à voix haute, caractère par caractère : on l'aère. */
  .fp {
    color: var(--accent);
    letter-spacing: 0.22em;
    font-size: 14px;
  }
</style>
