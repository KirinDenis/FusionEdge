// The radio's WiFi, sent over the USB cable the board was just flashed through (Web Serial).
// It talks the firmware's own serial LittleFS protocol (src/core/serial_littlefs.cpp): HELLO, BEGIN,
// /data/wifi.csv read, the new network put first, the file written back, REBOOT. The radio then
// starts, joins the network and prints its address, which this page shows. The name and password go
// to the board on the cable and nowhere else.

const BAUD = 460800;                 // Serial.begin() in src/main.cpp
const WIFI_FILE = '/data/wifi.csv';  // SSIDS_PATH in src/core/config.h: "name<TAB>password" a line
const MAX_NETS = 5;                  // Config::ssids[5]
const SSID_BYTES = 29;               // Config::initNetwork() keeps 29 bytes of a name
const PASS_BYTES = 39;               // and 39 of a password

const within = (promise, ms) => Promise.race([promise, new Promise((r) => {
  setTimeout(r, ms);
})]);

class Line {
  constructor(port) {
    this.port = port;
    this.lines = [];
    this.onLine = null;
    this.encoder = new TextEncoder();
  }

  async open() {
    try {
      await this.port.open({ baudRate: BAUD, bufferSize: 65536 });
    } catch (e) {
      throw new Error(`The port did not open (${e.message}). Close the flasher's window first, and any serial monitor: one port, one program at a time.`);
    }
    this.opened = true;
    // the browser opens a port with DTR and RTS on. RTS off first, then DTR: on the usual auto-reset
    // circuit EN is never pulled low that way, so opening does not restart the board
    await within(this.port.setSignals({ requestToSend: false }), 1000);
    await within(this.port.setSignals({ dataTerminalReady: false }), 1000);
    this.pumping = this.pump();
  }

  async pump() {
    const decoder = new TextDecoder();
    let text = '';
    while (this.port.readable && !this.closing) {
      this.reader = this.port.readable.getReader();
      try {
        for (;;) {
          const { value, done } = await this.reader.read();
          if (done) {
            this.closing = true;
            break;
          }
          text += decoder.decode(value, { stream: true });
          let i;
          while ((i = text.indexOf('\n')) >= 0) {
            const line = text.slice(0, i).replace(/\r$/, '');
            text = text.slice(i + 1);
            this.lines.push(line);
            if (this.lines.length > 400) {
              this.lines.shift();
            }
            this.onLine?.(line);
          }
        }
      } catch (e) {
        if (!['BreakError', 'BufferOverrunError', 'FramingError', 'ParityError'].includes(e.name)) {
          this.closing = true;               // the port went away; a restarting board's noise is not that
        }
      } finally {
        this.reader.releaseLock();
      }
    }
  }

  // a newline first: bytes left on the line by a restart would otherwise glue onto the command
  async send(line) {
    const writer = this.port.writable.getWriter();
    try {
      await writer.write(this.encoder.encode(`\n${line}\n`));
    } finally {
      writer.releaseLock();
    }
  }

  // the first line from now on that the test takes, or null after ms
  async expect(test, ms) {
    const until = Date.now() + ms;
    while (Date.now() < until) {
      const i = this.lines.findIndex(test);
      if (i >= 0) {
        return this.lines.splice(0, i + 1)[i];
      }
      await new Promise((r) => setTimeout(r, 40));
    }
    return null;
  }

  // a command of the protocol, and its answer: MRSPIFS|OK|<name>|... or MRSPIFS|ERR|<name>|...
  async ask(command, name, ms = 3000) {
    this.lines = [];
    await this.send(command);
    const line = await this.expect((l) => l.startsWith(`MRSPIFS|OK|${name}`) || l.startsWith(`MRSPIFS|ERR|${name}`), ms);
    if (!line) {
      throw new Error(`The radio did not answer ${name}.`);
    }
    if (line.startsWith('MRSPIFS|ERR|')) {
      throw new Error(`The radio said no to ${name}: ${line.split('|').slice(3).join('|')}.`);
    }
    return line;
  }

  async close() {
    this.closing = true;
    try {
      await within(this.reader?.cancel(), 1000);
    } catch (e) {
      console.warn(e);
    }
    await within(this.pumping, 1000);
    if (this.opened) {
      this.opened = false;
      try {
        await within(this.port.close(), 2000);
      } catch (e) {
        console.warn(e);
      }
    }
  }
}

const utf8 = (s) => new TextEncoder().encode(s);

function toBase64(bytes) {
  let s = '';
  for (const b of bytes) {
    s += String.fromCharCode(b);
  }
  return btoa(s);
}

function fromBase64(text) {
  const s = atob(text);
  const bytes = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) {
    bytes[i] = s.charCodeAt(i);
  }
  return bytes;
}

// what is wrong with a name and a password the radio would keep, or null
function badInput(ssid, pass) {
  if (!ssid) {
    return 'The network\'s name is empty.';
  }
  if (/[\t\r\n]/.test(ssid) || /[\t\r\n]/.test(pass)) {
    return 'A tab or a line break cannot be in a name or a password here.';
  }
  if (utf8(ssid).length > SSID_BYTES) {
    return `The radio keeps network names of up to ${SSID_BYTES} bytes.`;
  }
  if (utf8(pass).length > PASS_BYTES) {
    return `The radio keeps passwords of up to ${PASS_BYTES} bytes.`;
  }
  if (pass.length > 0 && pass.length < 8) {
    return 'A WiFi password has at least 8 characters (or none, for an open network).';
  }
  return null;
}

// the networks the radio knows, as its file has them: the passwords stay on this page and the board
async function readNets(line) {
  line.lines = [];
  await line.send(`READ|0|${WIFI_FILE}`);
  const first = await line.expect((l) => l.startsWith('MRSPIFS|READ_BEGIN|') || l.startsWith('MRSPIFS|ERR|READ'), 3000);
  if (!first || first.startsWith('MRSPIFS|ERR|')) {
    return [];                               // no file yet: a new radio
  }
  const chunks = [];
  for (;;) {
    const l = await line.expect((x) => x.startsWith('MRSPIFS|DATA|') || x.startsWith('MRSPIFS|OK|READ_END'), 3000);
    if (!l) {
      throw new Error('The radio stopped in the middle of its WiFi list.');
    }
    if (l.startsWith('MRSPIFS|OK|READ_END')) {
      break;
    }
    chunks.push(fromBase64(l.slice('MRSPIFS|DATA|'.length)));
  }
  const all = new Uint8Array(chunks.reduce((n, c) => n + c.length, 0));
  let at = 0;
  for (const c of chunks) {
    all.set(c, at);
    at += c.length;
  }
  const nets = [];
  for (const row of new TextDecoder().decode(all).split('\n')) {
    const tab = row.indexOf('\t');
    if (tab > 0) {
      nets.push({ ssid: row.slice(0, tab), pass: row.slice(tab + 1).replace(/\r$/, '') });
    }
  }
  return nets;
}

export function setupWifi(box) {
  const form = box.querySelector('form');
  const out = box.querySelector('.log');
  const button = form.querySelector('button');

  const say = (text, kind = '') => {
    const p = document.createElement('div');
    p.textContent = text;
    if (kind) {
      p.className = kind;
    }
    out.append(p);
    return p;
  };

  if (!('serial' in navigator)) {
    button.disabled = true;
    say('This browser cannot reach the cable. Chrome or Edge on a computer can; or use FusionEdge_AP below.', 'bad');
    return;
  }

  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    out.replaceChildren();
    const ssid = form.ssid.value.trim();
    const pass = form.pass.value;
    const wrong = badInput(ssid, pass);
    if (wrong) {
      say(wrong, 'bad');
      return;
    }
    let port;
    try {
      port = await navigator.serial.requestPort();
    } catch (x) {
      say('No port picked.', 'bad');
      return;
    }
    button.disabled = true;
    const line = new Line(port);
    try {
      await line.open();
      say('Looking for the radio on the cable...');
      let ready = null;
      for (let t = 0; t < 30 && !ready; t++) {   // a radio still starting answers in a few seconds
        line.lines = [];
        await line.send('HELLO');
        ready = await line.expect((l) => l.startsWith('MRSPIFS|OK|HELLO'), 700);
      }
      if (!ready) {
        throw new Error('The radio does not answer on the cable. Is FusionEdge on it, running? Its serial file access may be off in its settings: then use FusionEdge_AP below.');
      }
      await line.ask('BEGIN', 'BEGIN');
      say('Found it. Writing the WiFi list...');
      const nets = (await readNets(line)).filter((n) => n.ssid !== ssid);
      nets.unshift({ ssid, pass });
      const body = utf8(nets.slice(0, MAX_NETS).map((n) => `${n.ssid}\t${n.pass}\n`).join(''));
      await line.ask(`WRITE_BEGIN|0|${WIFI_FILE}|${body.length}`, 'WRITE_BEGIN');
      await line.ask(`WRITE_DATA|0|${toBase64(body)}`, 'WRITE_DATA');
      await line.ask('WRITE_END', 'WRITE_END');
      const kept = Math.min(nets.length, MAX_NETS);
      say(`Saved: ${ssid} first${kept > 1 ? `, and ${kept - 1} more it knew` : ''}. The radio restarts and joins it...`);
      line.lines = [];
      await line.send('REBOOT');
      const end = await line.expect((l) => /Web UI: http:\/\/[0-9.]+\//.test(l) || l.includes('Running in AP mode'), 60000);
      if (!end) {
        say('No word from the radio for a minute. Look at its screen: it shows its address once it is on the network.', 'bad');
      } else if (end.includes('Running in AP mode')) {
        say(`It did not get onto ${ssid}. Check the name and the password and send them again.`, 'bad');
      } else {
        const ip = end.match(/http:\/\/([0-9.]+)\//)[1];
        const p = say('The radio is on your network: ', 'good');
        const a = document.createElement('a');
        a.href = `http://${ip}/`;
        a.target = '_blank';
        a.rel = 'noopener';
        a.textContent = `http://${ip}/`;
        p.append(a);
      }
    } catch (x) {
      say(x.message, 'bad');
    } finally {
      await line.close();
      button.disabled = false;
      form.pass.value = '';
    }
  });
}
