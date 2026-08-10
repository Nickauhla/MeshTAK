import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.spinwaregames.meshtak',
  appName: 'MeshTAK',
  webDir: 'dist',
  // Pour le live-reload sur un téléphone physique, décommenter et remplacer par
  // l'IP LAN du poste de dev (celle qu'affiche `npm run dev`) :
  // server: { url: 'http://192.168.1.42:5173', cleartext: true },
  plugins: {
    BluetoothLe: {
      displayStrings: {
        scanning: 'Recherche du T-Beam…',
        cancel: 'Annuler',
        availableDevices: 'Modules détectés',
        noDeviceFound: 'Aucun T-Beam trouvé',
      },
    },
  },
};

export default config;
