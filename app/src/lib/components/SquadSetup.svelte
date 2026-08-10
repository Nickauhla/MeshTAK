<script lang="ts">
  import { DEFAULT_SQUAD_NAMES } from '../proto/frames.ts';
  import { session } from '../state/session.svelte.ts';

  let mode = $state<'choose' | 'create' | 'join'>('choose');
  let name = $state(DEFAULT_SQUAD_NAMES[0]);
  let busy = $state(false);

  async function create() {
    busy = true;
    await session.createSquad(name);
    busy = false;
  }

  async function join() {
    busy = true;
    await session.requestJoin(name);
    busy = false;
  }
</script>

<div class="setup">
  {#if session.isPending}
    <div class="beacon">
      <svg viewBox="0 0 80 80" aria-hidden="true">
        <circle class="w w1" cx="40" cy="40" r="12" />
        <circle class="w w2" cx="40" cy="40" r="12" />
        <circle class="hub" cx="40" cy="40" r="7" />
      </svg>
    </div>
    <h1>En attente de validation</h1>
    <p class="sub">
      Votre demande pour rejoindre <b>{name}</b> est diffusée toutes les 5 secondes. Le chef d'escouade
      doit l'accepter sur son téléphone.
    </p>
    <p class="hint">
      Il verra votre indicatif et votre empreinte — demandez-lui de vérifier qu'elle correspond à celle
      affichée dans vos réglages.
    </p>
    {#if session.joinNotice}
      <p class="notice">{session.joinNotice}</p>
    {/if}
    <button class="btn-ghost wide" onclick={() => session.leaveSquad()}>Annuler</button>
  {:else if mode === 'choose'}
    <div class="mark">◈</div>
    <h1>Aucune escouade</h1>
    <p class="sub">
      Une escouade est un groupe fermé : ses échanges sont chiffrés et illisibles pour les autres
      joueurs, même s'ils relaient vos trames.
    </p>
    <button class="btn-primary wide" onclick={() => (mode = 'create')}>Créer une escouade</button>
    <button class="btn-ghost wide" onclick={() => (mode = 'join')}>Rejoindre une escouade</button>
    {#if session.joinNotice}
      <p class="notice">{session.joinNotice}</p>
    {/if}
  {:else}
    <h1>{mode === 'create' ? 'Créer une escouade' : 'Rejoindre une escouade'}</h1>

    <div class="names">
      {#each DEFAULT_SQUAD_NAMES as n (n)}
        <button class="chip" class:selected={name === n} onclick={() => (name = n)}>{n}</button>
      {/each}
    </div>
    <input class="mono" bind:value={name} maxlength="12" placeholder="Nom de l'escouade" />

    {#if mode === 'create'}
      <p class="hint">
        Vous en serez le <b>chef</b> : votre boîtier tire une clé au hasard et c'est vous qui validerez
        chaque joueur qui demandera à rejoindre.
      </p>
      <button class="btn-primary wide" onclick={create} disabled={busy || !name.trim()}>
        Créer {name}
      </button>
    {:else}
      <p class="hint">
        Le nom doit être exactement celui annoncé par le chef. Votre demande sera diffusée par radio ;
        il devra l'accepter.
      </p>
      <button class="btn-primary wide" onclick={join} disabled={busy || !name.trim()}>
        Demander à rejoindre {name}
      </button>
    {/if}

    <button class="btn-ghost wide" onclick={() => (mode = 'choose')}>Retour</button>
  {/if}
</div>

<style>
  .setup {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 11px;
    padding: 32px 24px;
    text-align: center;
    overflow-y: auto;
  }
  .mark {
    font-size: 38px;
    color: var(--accent);
    line-height: 1;
    text-shadow: 0 0 14px rgba(155, 217, 60, 0.45);
  }

  /* Balise : deux ondes qui partent du centre — la demande est en émission. */
  .beacon svg {
    width: 74px;
    height: 74px;
  }
  .hub {
    fill: var(--warn);
  }
  .w {
    fill: none;
    stroke: var(--warn);
    stroke-width: 2;
    transform-origin: 40px 40px;
    animation: wave 2.6s ease-out infinite;
  }
  .w2 {
    animation-delay: 1.3s;
  }
  @keyframes wave {
    from {
      transform: scale(0.6);
      opacity: 0.9;
    }
    to {
      transform: scale(3);
      opacity: 0;
    }
  }

  h1 {
    margin: 4px 0 0;
    font-size: 19px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.16em;
  }
  .sub {
    color: var(--muted);
    font-size: 13px;
    margin: 0 0 8px;
    max-width: 300px;
    line-height: 1.55;
  }
  .hint {
    color: var(--muted);
    font-size: 12px;
    max-width: 300px;
    line-height: 1.5;
    margin: 2px 0 4px;
    border-left: 2px solid var(--line);
    padding-left: 10px;
    text-align: left;
  }
  b {
    color: var(--fg);
  }
  .notice {
    color: var(--warn);
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    border: 1px solid rgba(224, 161, 42, 0.35);
    background: rgba(224, 161, 42, 0.07);
    padding: 7px 12px;
    margin: 2px 0;
  }

  /* Noms prédéfinis : des plaques, pas des pastilles. */
  .names {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    justify-content: center;
    max-width: 330px;
    margin-top: 6px;
  }
  /* Le nom s'affiche tel qu'il partira sur la radio — la casse compte des deux
     côtés, donc pas de capitales décoratives ici. */
  .chip {
    background: var(--surface);
    border: 1px solid var(--line);
    color: var(--muted);
    padding: 7px 13px;
    font-size: 12px;
    letter-spacing: 0.12em;
    text-transform: none;
  }
  .chip.selected {
    border-color: var(--accent);
    color: var(--accent);
    background: var(--accent-soft);
  }
  /* Pas de capitales forcées : le nom doit être lu tel qu'il sera comparé, la
     casse comptant des deux côtés de la radio. */
  input {
    text-align: center;
    letter-spacing: 0.16em;
    width: 250px;
    flex: none;
  }
  .wide {
    min-width: 250px;
    padding: 13px 22px;
  }
</style>
