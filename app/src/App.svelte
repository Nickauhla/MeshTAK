<script lang="ts">
  import ChatView from './lib/components/ChatView.svelte';
  import ConnectView from './lib/components/ConnectView.svelte';
  import JoinRequests from './lib/components/JoinRequests.svelte';
  import MapView from './lib/components/MapView.svelte';
  import RosterView from './lib/components/RosterView.svelte';
  import SettingsView from './lib/components/SettingsView.svelte';
  import SquadSetup from './lib/components/SquadSetup.svelte';
  import { session } from './lib/state/session.svelte.ts';

  type Tab = 'map' | 'squad' | 'chat' | 'settings';
  let tab = $state<Tab>('map');
  let everConnected = $state(false);

  $effect(() => {
    if (session.connected) everConnected = true;
  });

  // Icônes tracées : aucun fichier de police à embarquer, et le trait reste net
  // à toutes les densités d'écran.
  const TABS: { id: Tab; label: string; path: string }[] = [
    { id: 'map', label: 'Carte', path: 'M3 6l6-3 6 3 6-3v15l-6 3-6-3-6 3zM9 3v15M15 6v18' },
    {
      id: 'squad',
      label: 'Escouade',
      path: 'M9 11a3.2 3.2 0 100-6.4A3.2 3.2 0 009 11zM2.5 20c0-3.2 2.9-5.6 6.5-5.6s6.5 2.4 6.5 5.6M16.5 9.4a2.7 2.7 0 100-5.4M18 13.6c2.2.6 3.5 2.2 3.5 4.4',
    },
    { id: 'chat', label: 'Messages', path: 'M4 4.5h16v11H9.5L4 20zM8 9h8M8 12h5' },
    {
      id: 'settings',
      label: 'Réglages',
      path: 'M3 7h9M17 7h4M3 17h5M14 17h7M14.5 7a2.5 2.5 0 105 0 2.5 2.5 0 10-5 0M8 17a2.5 2.5 0 105 0 2.5 2.5 0 10-5 0',
    },
  ];

  const unread = $derived(session.messages.filter((m) => !m.outgoing).length);
</script>

{#if !everConnected}
  <ConnectView />
{:else}
  <header>
    <div class="left">
      <span class="link" class:on={session.connected}>
        <span class="dot"></span>
      </span>
      <div class="ident">
        <strong>{session.config.squadName || 'sans escouade'}</strong>
        <small class="mono">
          {session.config.callsign}
          {#if session.inSquad}<span class="sep">/</span>#{session.config.addr}{/if}
          {#if session.isLeader}<span class="chef">CHEF</span>{/if}
        </small>
      </div>
    </div>
    <div class="right">
      {#if session.status}
        <div class="gauge" class:warn={!session.status.fixValid}>
          <span class="k">gnss</span>
          <span class="v mono">
            {session.status.fixValid ? `${session.status.sats}` : '––'}
          </span>
        </div>
        <div class="gauge">
          <span class="k">batt</span>
          <span class="v mono">
            {session.status.battPct === 255 ? '––' : session.status.battPct + '%'}
          </span>
        </div>
      {:else}
        <button class="btn-ghost small" onclick={() => session.connect()}>Reconnecter</button>
      {/if}
    </div>
  </header>

  {#if !session.inSquad}
    <SquadSetup />
  {:else}
    <JoinRequests />

    <main>
      {#if tab === 'map'}
        <MapView />
      {:else if tab === 'squad'}
        <RosterView />
      {:else if tab === 'chat'}
        <ChatView />
      {:else}
        <SettingsView />
      {/if}
    </main>

    <nav>
      {#each TABS as t (t.id)}
        <button class:active={tab === t.id} onclick={() => (tab = t.id)}>
          <svg
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="1.7"
            stroke-linecap="round"
            stroke-linejoin="round"
          >
            <path d={t.path} />
          </svg>
          <span class="label">{t.label}</span>
          {#if t.id === 'chat' && unread > 0 && tab !== 'chat'}
            <span class="badge mono">{unread}</span>
          {/if}
        </button>
      {/each}
    </nav>
  {/if}
{/if}

<style>
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    padding: 8px 14px 9px;
    padding-top: calc(8px + env(safe-area-inset-top));
    border-bottom: 1px solid var(--line);
    background: linear-gradient(180deg, #0d120c, #080b08);
    flex: none;
    position: relative;
  }
  /* Filet phosphore en tête d'écran : le repère haut de l'appareil. */
  header::after {
    content: '';
    position: absolute;
    left: 0;
    right: 0;
    bottom: -1px;
    height: 1px;
    background: linear-gradient(90deg, var(--accent), transparent 55%);
    opacity: 0.55;
  }
  .left,
  .right {
    display: flex;
    align-items: center;
    gap: 10px;
    min-width: 0;
  }

  /* Témoin de liaison : le point bat quand le lien est établi. */
  .link {
    width: 18px;
    height: 18px;
    border: 1px solid var(--line);
    border-radius: 50%;
    display: grid;
    place-items: center;
    flex: none;
  }
  .dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--danger);
  }
  .link.on {
    border-color: rgba(155, 217, 60, 0.45);
  }
  .link.on .dot {
    background: var(--accent);
    box-shadow: 0 0 7px var(--accent);
    animation: pulse 2.4s ease-in-out infinite;
  }
  @keyframes pulse {
    50% {
      opacity: 0.35;
    }
  }

  .ident {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }
  .ident strong {
    font-size: 15px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.13em;
    line-height: 1.15;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .ident small {
    font-size: 11px;
    color: var(--muted);
    letter-spacing: 0.04em;
  }
  .sep {
    color: var(--dim);
    margin: 0 3px;
  }
  .chef {
    color: var(--accent);
    margin-left: 6px;
    letter-spacing: 0.12em;
  }

  /* Jauges : intitulé minuscule, valeur en chasse fixe — lisible d'un coup d'œil. */
  .gauge {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    line-height: 1.1;
  }
  .gauge .k {
    font-size: 9px;
    text-transform: uppercase;
    letter-spacing: 0.18em;
    color: var(--dim);
  }
  .gauge .v {
    font-size: 14px;
    font-weight: 500;
  }
  .gauge.warn .v {
    color: var(--warn);
  }
  header button.small {
    font-size: 11px;
    padding: 6px 10px;
  }

  main {
    flex: 1;
    display: flex;
    flex-direction: column;
    min-height: 0;
  }

  nav {
    display: flex;
    border-top: 1px solid var(--line);
    background: #0b0f0a;
    padding-bottom: env(safe-area-inset-bottom);
    flex: none;
  }
  nav button {
    flex: 1;
    background: none;
    border: none;
    border-radius: 0;
    color: var(--dim);
    padding: 8px 0 7px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    position: relative;
    letter-spacing: 0.1em;
  }
  nav button svg {
    width: 21px;
    height: 21px;
  }
  /* Onglet actif : marqué par un bandeau haut, pas seulement par la couleur. */
  nav button.active {
    color: var(--accent);
    background: linear-gradient(180deg, var(--accent-soft), transparent 70%);
  }
  nav button.active::before {
    content: '';
    position: absolute;
    top: -1px;
    left: 12%;
    right: 12%;
    height: 2px;
    background: var(--accent);
  }
  .label {
    font-size: 10px;
    font-weight: 600;
  }
  .badge {
    position: absolute;
    top: 3px;
    right: 50%;
    transform: translateX(20px);
    background: var(--danger);
    color: #fff;
    font-size: 10px;
    min-width: 16px;
    height: 15px;
    border-radius: 2px;
    display: grid;
    place-items: center;
    padding: 0 3px;
    letter-spacing: 0;
  }
</style>
