<script lang="ts">
  import { session } from '../state/session.svelte.ts';

  let open = $state(false);

  // Une demande d'adhésion est un acte de terrain : dès qu'il y en a une, elle
  // s'impose au chef, quel que soit l'onglet où il se trouve.
  $effect(() => {
    if (session.candidates.length > 0) open = true;
  });
</script>

{#if session.isLeader && session.candidates.length > 0}
  <button class="bar" onclick={() => (open = !open)}>
    <span class="n mono">{session.candidates.length}</span>
    demande{session.candidates.length > 1 ? 's' : ''} d'adhésion
    <span class="chev">{open ? '▾' : '▸'}</span>
  </button>

  {#if open}
    <div class="list">
      {#each session.candidates as c (c.fingerprint)}
        <div class="row">
          <div class="who">
            <strong>{c.callsign || 'sans indicatif'}</strong>
            <small>empreinte <code class="mono">{c.fingerprint}</code></small>
          </div>
          <div class="acts">
            <button class="ok" onclick={() => session.acceptCandidate(c.fingerprint)}>Accepter</button>
            <button class="no" onclick={() => session.refuseCandidate(c.fingerprint)}>Refuser</button>
          </div>
        </div>
      {/each}
      <p class="hint">
        Faites confirmer l'empreinte de vive voix avant d'accepter : c'est le seul moyen de savoir que
        vous validez la bonne personne et non quelqu'un qui a repris son indicatif.
      </p>
    </div>
  {/if}
{/if}

<style>
  /* Bandeau d'alerte : hachures d'avertissement, comme un marquage d'équipement. */
  .bar {
    width: 100%;
    background:
      repeating-linear-gradient(
        -45deg,
        rgba(0, 0, 0, 0.18) 0 8px,
        transparent 8px 16px
      ),
      #8a5c00;
    border: none;
    border-bottom: 1px solid var(--line);
    border-radius: 0;
    color: #fff9ec;
    padding: 8px 14px;
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 0.12em;
    display: flex;
    align-items: center;
    gap: 8px;
    flex: none;
  }
  .n {
    background: #fff9ec;
    color: #5a3c00;
    padding: 1px 6px;
    font-size: 12px;
    letter-spacing: 0;
  }
  .chev {
    margin-left: auto;
    opacity: 0.8;
  }

  .list {
    background: var(--surface);
    border-bottom: 1px solid var(--line);
    padding: 8px 14px 12px;
    flex: none;
  }
  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    padding: 9px 0;
    border-bottom: 1px solid var(--line-soft);
  }
  .who {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }
  .who strong {
    font-size: 14px;
    text-transform: uppercase;
    letter-spacing: 0.1em;
  }
  small {
    color: var(--dim);
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 0.12em;
  }
  code {
    color: var(--warn);
    font-size: 12px;
    letter-spacing: 0.14em;
    text-transform: none;
  }
  .acts {
    display: flex;
    gap: 6px;
    flex: none;
  }
  .acts button {
    padding: 8px 12px;
    font-size: 11px;
    letter-spacing: 0.1em;
  }
  .acts .ok {
    background: var(--accent);
    border-color: var(--accent);
    color: var(--accent-ink);
    font-weight: 700;
  }
  .acts .no {
    background: transparent;
    color: var(--danger);
    border-color: rgba(224, 72, 58, 0.45);
  }
  .hint {
    color: var(--muted);
    font-size: 11px;
    line-height: 1.55;
    margin: 9px 0 0;
  }
</style>
