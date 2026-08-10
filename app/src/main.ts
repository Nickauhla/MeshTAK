import { mount } from 'svelte';

import App from './App.svelte';
import './app.css';
import { session } from './lib/state/session.svelte.ts';

session.init();

export default mount(App, { target: document.getElementById('app')! });
