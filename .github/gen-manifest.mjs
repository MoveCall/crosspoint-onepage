#!/usr/bin/env node
// gen-manifest.mjs — emit an esp-web-tools manifest.json for the OnePage web flasher.
//
// The website (onepage-reader-web) loads this repo's
//   releases/latest/download/manifest.json
// and flashes the .bin it points at. Because the website only ever reads
// `latest/download/manifest.json` (a fixed name whose *content* changes per
// release), the .bin can carry a version number and the website never changes.
//
// Usage:
//   node gen-manifest.mjs --version v0.2.0 --bin onepage-firmware-v0.2.0.bin
//
// Both the generated manifest.json and the versioned .bin must be uploaded as
// assets on the SAME release, so `latest/download/<name>` resolves for each.

import { writeFileSync } from 'node:fs';

const args = Object.fromEntries(
  process.argv.slice(2).reduce((acc, a, i, arr) => {
    if (a.startsWith('--')) acc.push([a.slice(2), arr[i + 1]]);
    return acc;
  }, [])
);

const version = args.version;
const bin = args.bin;
if (!version || !bin) {
  console.error('Usage: node gen-manifest.mjs --version vX.Y.Z --bin <file>.bin');
  process.exit(1);
}

const REPO = 'MoveCall/crosspoint-onepage';
// Resolve to the .bin on THIS release. `latest/download/<bin>` also works, but
// pinning the tag keeps a manifest reproducible if you ever inspect an old one.
const binUrl = `https://github.com/${REPO}/releases/download/${version}/${bin}`;

const manifest = {
  name: 'OnePage',
  version: version.replace(/^v/, ''),
  new_install_prompt_erase: false,
  builds: [
    {
      // esptool-js recognises ESP32-C61 (magic 0x4f81606f → ESP32C61ROM).
      chipFamily: 'ESP32-C61',
      parts: [{ path: binUrl, offset: 0 }],
    },
  ],
};

writeFileSync('manifest.json', JSON.stringify(manifest, null, 2) + '\n');
console.log(`Wrote manifest.json → ${binUrl}`);
