<script lang="ts">
  import { session } from '../state/session.svelte.ts';

  let draft = $state('');
  /** undefined = diffusion à toute l'escouade. */
  let target = $state<number | undefined>(undefined);
  let listEl: HTMLDivElement;

  const QUICK = ['Contact !', 'RAS', 'Je me replie', 'Besoin de renfort', 'En position'];

  async function send(text: string) {
    if (!text.trim()) return;
    await session.sendText(text, target);
    draft = '';
    queueMicrotask(() => listEl?.scrollTo({ top: listEl.scrollHeight, behavior: 'smooth' }));
  }

  function time(ts: number): string {
    return new Date(ts).toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit' });
  }

  // Auto-scroll à l'arrivée d'un message.
  $effect(() => {
    session.messages.length;
    queueMicrotask(() => listEl?.scrollTo({ top: listEl.scrollHeight }));
  });
</script>

<div class="chat">
  <div class="target">
    <label for="dest">Dest.</label>
    <select id="dest" bind:value={target}>
      <option value={undefined}>Toute l'escouade — {session.config.squadName}</option>
      {#each session.squadMates as node (node.addr)}
        <option value={node.addr}>{node.callsign} (#{node.addr})</option>
      {/each}
    </select>
  </div>

  <div class="messages" bind:this={listEl}>
    {#if session.messages.length === 0}
      <p class="empty">
        Aucun trafic. Les messages passent par la radio LoRa, pas par le réseau cellulaire.
      </p>
    {/if}
    {#each session.messages as m (m.key)}
      <div class="msg" class:mine={m.outgoing} class:priv={m.to !== undefined}>
        <div class="meta mono">
          <span class="from">{m.outgoing ? 'MOI' : m.fromName}</span>
          <span class="t">{time(m.at)}</span>
          {#if m.to !== undefined}<span class="flag">privé</span>{/if}
          {#if m.outgoing && m.to !== undefined}
            <span class="ack" class:ok={m.acked}>{m.acked ? '✓ reçu' : '⋯ attente'}</span>
          {/if}
        </div>
        <div class="body">{m.text}</div>
      </div>
    {/each}
  </div>

  <div class="quick">
    {#each QUICK as q (q)}
      <button onclick={() => send(q)} disabled={!session.connected}>{q}</button>
    {/each}
  </div>

  <form
    class="composer"
    onsubmit={(e) => {
      e.preventDefault();
      send(draft);
    }}
  >
    <input
      bind:value={draft}
      placeholder={session.connected ? 'Message…' : 'T-Beam non connecté'}
      disabled={!session.connected}
      maxlength="180"
    />
    <button class="btn-primary" type="submit" disabled={!session.connected || !draft.trim()}>
      Envoyer
    </button>
  </form>
</div>

<style>
  .chat {
    flex: 1;
    display: flex;
    flex-direction: column;
    min-height: 0;
  }

  /* Sélecteur de destinataire : bandeau d'en-tête, pas un champ de formulaire. */
  .target {
    padding: 9px 14px;
    border-bottom: 1px solid var(--line);
    background: var(--surface);
    display: flex;
    align-items: center;
    gap: 10px;
    flex: none;
  }
  .target label {
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 0.16em;
    color: var(--dim);
    white-space: nowrap;
  }
  select {
    font-size: 13px;
    padding: 7px 9px;
    background: #0a0e0a;
  }

  .messages {
    flex: 1;
    overflow-y: auto;
    padding: 14px;
    display: flex;
    flex-direction: column;
    gap: 9px;
  }

  /* Trafic reçu : bloc barré à gauche. Trafic émis : barré à droite, en
     phosphore — l'origine se lit sans lire le libellé. */
  .msg {
    max-width: 84%;
    background: var(--surface);
    border: 1px solid var(--line-soft);
    border-left: 3px solid var(--line);
    padding: 7px 11px;
    align-self: flex-start;
  }
  .msg.mine {
    align-self: flex-end;
    border-left: 1px solid var(--line-soft);
    border-right: 3px solid var(--accent);
    background: var(--surface-2);
  }
  .msg.priv {
    border-left-color: var(--warn);
  }
  .msg.mine.priv {
    border-left-color: var(--line-soft);
    border-right-color: var(--warn);
  }
  .meta {
    display: flex;
    gap: 8px;
    font-size: 10px;
    color: var(--dim);
    margin-bottom: 3px;
    align-items: center;
    text-transform: uppercase;
    letter-spacing: 0.1em;
  }
  .from {
    color: var(--muted);
    font-weight: 500;
  }
  .msg.mine .from {
    color: var(--accent);
  }
  .flag {
    color: var(--warn);
  }
  .ack.ok {
    color: var(--ok);
  }
  .body {
    font-size: 15px;
    line-height: 1.4;
    word-break: break-word;
  }

  /* Phrases prêtes : la voie rapide quand on ne peut pas taper. */
  .quick {
    display: flex;
    gap: 6px;
    padding: 8px 14px 0;
    overflow-x: auto;
    flex: none;
    scrollbar-width: none;
  }
  .quick button {
    white-space: nowrap;
    background: var(--surface);
    border-color: var(--line);
    color: var(--muted);
    padding: 7px 12px;
    font-size: 11px;
    letter-spacing: 0.1em;
  }

  .composer {
    display: flex;
    gap: 8px;
    padding: 10px 14px;
    border-top: 1px solid var(--line);
    background: var(--surface);
    flex: none;
  }
  .composer button {
    padding: 0 18px;
    font-size: 13px;
    flex: none;
  }
  .empty {
    color: var(--muted);
    font-size: 12.5px;
    margin: auto;
    text-align: center;
    max-width: 250px;
    line-height: 1.6;
  }
</style>
