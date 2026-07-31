/* tools/serve.js — OPTIONAL localhost launcher for SHITFIRE.
   Run: node tools/serve.js   (or double-click tools/serve.cmd)

   Why this exists: Chrome cannot persist microphone permission for a file://
   page, so the double-clicked SHITFIRE.html asks once per session even after
   the in-page mic hold. Served from http://localhost, Chrome has a real
   origin to remember — grant the mic ONCE and it persists forever.

   This is a CONVENIENCE, not a requirement: the golden rule stands, the
   artifact opens by double-click with nothing installed, and this server
   adds no dependency (node stdlib only, dev machines already have node for
   the build). READ-ONLY against the repo per the dev-tooling rules. */
import http from 'http';
import fs from 'fs';
import path from 'path';
import { exec } from 'child_process';
import { fileURLToPath } from 'url';

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const PORT = 8137;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.png': 'image/png',
               '.json': 'application/json', '.md': 'text/plain', '.css': 'text/css' };

const server = http.createServer((req, res) => {
  const urlPath = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  const rel = urlPath === '/' ? 'SHITFIRE.html' : urlPath.slice(1);
  const file = path.normalize(path.join(root, rel));
  if (!file.startsWith(path.normalize(root))) { res.writeHead(403); res.end(); return; }
  fs.readFile(file, (err, data) => {
    if (err) { res.writeHead(404); res.end('not found'); return; }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
    res.end(data);
  });
});
server.listen(PORT, '127.0.0.1', () => {
  const url = `http://localhost:${PORT}/SHITFIRE.html`;
  console.log(`SHITFIRE serving at ${url}`);
  console.log('Grant the microphone once on this origin and Chrome remembers it permanently.');
  console.log('Ctrl+C stops the server; the double-clicked SHITFIRE.html keeps working without it.');
  exec(`start "" "${url}"`);   // Windows: open the default browser
});
