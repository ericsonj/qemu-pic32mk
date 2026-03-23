#!/usr/bin/env python3
"""
gpio_tool.py — Host-side GPIO control for PIC32MK QEMU emulator via QMP/QOM.

Uses QEMU's built-in qom-get / qom-set commands over the QMP socket.
No extra dependencies beyond Python 3.6+ standard library.

QOM paths (registered as named children of the machine object):
  PORTA → /machine/gpio-portA
  PORTB → /machine/gpio-portB
  ...
  PORTG → /machine/gpio-portG

Available QOM properties on each gpio-portX device:
  pin0 … pin15   bool  r/w  inject external input (w) / read PORT bit (r)
  lat-state       u32   r    current LAT register (output latch)
  port-state      u32   r    current PORT register (mixed in/out)
  tris-state      u32   r    current TRIS register (1=input, 0=output)

Usage:
  # Start QEMU with QMP socket:
  #   -qmp unix:/tmp/qemu-qmp.sock,server=on,wait=off
  #   (or: -monitor stdio  and use HMP: qom-get / qom-set)

  python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock set A 7 1
  python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock get A 7
  python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock get-port A
  python3 gpio_tool.py --qmp /tmp/qemu-qmp.sock list-paths
"""

import argparse
import json
import socket
import sys

PORT_NAMES = {
    'A': 'gpio-portA', 'B': 'gpio-portB', 'C': 'gpio-portC',
    'D': 'gpio-portD', 'E': 'gpio-portE', 'F': 'gpio-portF',
    'G': 'gpio-portG',
}


class QMPClient:
    """Minimal QMP client over a UNIX or TCP socket."""

    def __init__(self, path: str):
        if path.startswith('tcp:'):
            # tcp:host:port
            _, host, port = path.split(':', 2)
            self._sock = socket.create_connection((host, int(port)))
        else:
            self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._sock.connect(path)
        self._buf = b''
        # Read and discard the QMP greeting banner
        self._read_line()
        # Send capabilities negotiation
        self._send({'execute': 'qmp_capabilities'})
        self._read_response()

    def _send(self, obj: dict):
        data = json.dumps(obj).encode() + b'\n'
        self._sock.sendall(data)

    def _read_line(self) -> str:
        while b'\n' not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise ConnectionError('QMP socket closed')
            self._buf += chunk
        line, self._buf = self._buf.split(b'\n', 1)
        return line.decode()

    def _read_response(self) -> dict:
        """Read lines until we get a 'return' or 'error' key."""
        while True:
            line = self._read_line()
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if 'return' in obj or 'error' in obj:
                return obj
            # 'event' lines — ignore

    def execute(self, cmd: str, **args) -> object:
        req = {'execute': cmd}
        if args:
            req['arguments'] = args
        self._send(req)
        resp = self._read_response()
        if 'error' in resp:
            raise RuntimeError(f"QMP error: {resp['error']}")
        return resp['return']

    def qom_get(self, path: str, prop: str) -> object:
        return self.execute('qom-get', path=path, property=prop)

    def qom_set(self, path: str, prop: str, value: object):
        self.execute('qom-set', path=path, property=prop, value=value)

    def qom_list(self, path: str) -> list:
        return self.execute('qom-list', path=path)

    def close(self):
        self._sock.close()


def gpio_path(port_letter: str) -> str:
    name = PORT_NAMES.get(port_letter.upper())
    if name is None:
        raise ValueError(f"Unknown port '{port_letter}'. Use A–G.")
    return f'/machine/{name}'


def cmd_set(qmp: QMPClient, port: str, pin: int, value: int):
    """Inject external input on a pin (0 or 1)."""
    path = gpio_path(port)
    prop = f'pin{pin}'
    qmp.qom_set(path, prop, bool(value))
    print(f'PORT{port.upper()} pin{pin} input → {value}')


def cmd_get(qmp: QMPClient, port: str, pin: int):
    """Read current PORT bit for a pin."""
    path = gpio_path(port)
    prop = f'pin{pin}'
    val = qmp.qom_get(path, prop)
    print(f'PORT{port.upper()} pin{pin} = {int(val)}')


def cmd_get_port(qmp: QMPClient, port: str):
    """Read lat-state, port-state, tris-state for the whole port."""
    path = gpio_path(port)
    lat  = qmp.qom_get(path, 'lat-state')
    port_val = qmp.qom_get(path, 'port-state')
    tris = qmp.qom_get(path, 'tris-state')
    p = port.upper()
    print(f'PORT{p}:')
    print(f'  TRIS  = 0x{tris:04X}  (1=input, 0=output)')
    print(f'  LAT   = 0x{lat:04X}  (output latch)')
    print(f'  PORT  = 0x{port_val:04X}  (current pin state)')
    # Pretty-print per-pin summary
    print(f'  {"pin":>4}  {"dir":>5}  {"lat":>4}  {"port":>5}')
    for i in range(15, -1, -1):
        direction = 'IN ' if (tris >> i) & 1 else 'OUT'
        lv = (lat >> i) & 1
        pv = (port_val >> i) & 1
        print(f'  {i:>4}  {direction:>5}  {lv:>4}  {pv:>5}')


def cmd_list_paths(qmp: QMPClient):
    """List QOM paths and properties for all GPIO port devices."""
    for letter, name in PORT_NAMES.items():
        path = f'/machine/{name}'
        try:
            props = qmp.qom_list(path)
            print(f'{path}  (PORT{letter})')
            for p in props:
                print(f'  {p["name"]:20s}  {p.get("type", "")}')
        except RuntimeError as e:
            print(f'{path}  ERROR: {e}')


# ---------------------------------------------------------------------------
# ADC analog value commands
# ---------------------------------------------------------------------------

def cmd_adc_set(qmp: QMPClient, channel: int, value: int):
    """Inject analog value (0–4095) for an ADC channel."""
    prop = f'adc-ch{channel}'
    qmp.qom_set(ADC_QOM_PATH, prop, value)
    print(f'ADC channel {channel} input ← {value}')


def cmd_adc_get(qmp: QMPClient, channel: int):
    """Read last conversion result for an ADC channel."""
    prop = f'adc-data{channel}'
    val = qmp.qom_get(ADC_QOM_PATH, prop)
    prop_in = f'adc-ch{channel}'
    inp = qmp.qom_get(ADC_QOM_PATH, prop_in)
    print(f'ADC channel {channel}: input={inp}, data={val}')


def cmd_adc_list(qmp: QMPClient):
    """Show all mapped VOLTU analog channels with input and data values."""
    print(f'{"Ch":>4}  {"Signal":18s}  {"Input":>6}  {"Data":>6}')
    print(f'{"-"*4:>4}  {"-"*18:18s}  {"-"*6:>6}  {"-"*6:>6}')
    for ch in sorted(VOLTU_ANALOG.keys()):
        sig, unit, _, _ = VOLTU_ANALOG[ch]
        try:
            inp = qmp.qom_get(ADC_QOM_PATH, f'adc-ch{ch}')
            data = qmp.qom_get(ADC_QOM_PATH, f'adc-data{ch}')
        except RuntimeError:
            inp = data = '?'
        print(f'{ch:>4}  {sig:18s}  {inp:>6}  {data:>6}')


# ---------------------------------------------------------------------------
# VOLTU pin name mapping: (port_letter, pin) -> (signal_name, direction_hint)
# Only pins in this dict appear in the GUI.  Source: plib_gpio.h
# ---------------------------------------------------------------------------

VOLTU_PINS = {
    # Port A
    ('A',  0): ('WDO_MB',           'OUT'),
    ('A',  1): ('WDI_MB',           'OUT'),
    ('A',  7): ('SPI_EN',           'IN'),
    ('A', 10): ('REL_PUMP_IPU',     'OUT'),
    ('A', 14): ('GND_CVH',          'OUT'),
    ('A', 15): ('GND_DIFF_LOCK',    'OUT'),
    # Port B
    ('B',  6): ('BTN_UP',           'IN'),
    ('B',  7): ('BTN_MAN',          'IN'),
    ('B', 10): ('GND_AC_CLUTCH',    'OUT'),
    ('B', 11): ('REL_VACUUM',       'OUT'),
    ('B', 12): ('REL_PUMP_STEER',   'OUT'),
    ('B', 13): ('REL_FAN',          'OUT'),
    ('B', 15): ('KEY_RUN',          'IN'),
    # Port C
    ('C',  0): ('BCM_WUP',          'IN'),
    ('C', 10): ('BTN_DOWN',         'IN'),
    # Port D
    ('D',  1): ('KEY_START',        'IN'),
    ('D',  2): ('BRAKE_NO',         'IN'),
    ('D',  3): ('BRAKE_NC',         'IN'),
    ('D',  4): ('WUP_BMS',          'OUT'),
    # Port E
    ('E',  8): ('HVIL_FRONT',       'IN'),
    ('E',  9): ('HVIL_IDU',         'IN'),
    # Port G
    ('G',  8): ('CRASH',            'IN'),
    ('G', 10): ('HVIL_REAR',        'IN'),
    ('G', 12): ('GND_PUMP',         'OUT'),
    ('G', 13): ('REL_PUMP_IEP',     'OUT'),
    ('G', 14): ('GND_VALVE',        'OUT'),
    ('G', 15): ('NFLT_BMS',         'IN'),
}

PORT_INDEX_TO_LETTER = {0: 'A', 1: 'B', 2: 'C', 3: 'D', 4: 'E', 5: 'F', 6: 'G'}

ADC_QOM_PATH = '/machine/adchs'

# ---------------------------------------------------------------------------
# VOLTU analog channel mapping: channel -> (signal_name, unit, min, max)
# Source: Harmony MHC ADC configuration for PRG-IDU
# ---------------------------------------------------------------------------

VOLTU_ANALOG = {
    10: ('V_BAT_SENSE',    'mV', 0, 4095),
    15: ('TEMP_MOTOR',     'raw', 0, 4095),
    16: ('TEMP_INV',       'raw', 0, 4095),
    17: ('I_PUMP_STEER',   'raw', 0, 4095),
    18: ('I_FAN',          'raw', 0, 4095),
    19: ('I_PUMP_IPU',     'raw', 0, 4095),
    20: ('I_PUMP_IEP',     'raw', 0, 4095),
    21: ('I_VACUUM',       'raw', 0, 4095),
}


# ---------------------------------------------------------------------------
# GUI mode — event-driven tkinter interface
# ---------------------------------------------------------------------------

def cmd_gui(qmp: QMPClient, gpio_sock_path: str):
    """Launch the tkinter GPIO monitor GUI."""
    try:
        import tkinter as tk
    except ImportError:
        print('Error: tkinter required for GUI mode. Install python3-tk.',
              file=sys.stderr)
        sys.exit(1)

    import struct
    import threading
    import queue

    LED_ON  = '#00cc00'
    LED_OFF = '#444444'

    # -- Socket listener thread -----------------------------------------------

    class SocketListenerThread(threading.Thread):
        """Reads 13-byte GPIO state messages from the chardev socket."""
        MSG_LEN = 13

        def __init__(self, sock_path: str, q: queue.Queue):
            super().__init__(daemon=True)
            self._path = sock_path
            self._q = q
            self._stop_evt = threading.Event()

        def stop(self):
            self._stop_evt.set()

        def run(self):
            try:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.connect(self._path)
                sock.settimeout(0.5)
            except OSError as e:
                self._q.put(('error', str(e)))
                return

            buf = b''
            while not self._stop_evt.is_set():
                try:
                    data = sock.recv(4096)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not data:
                    break
                buf += data
                while len(buf) >= self.MSG_LEN:
                    msg = buf[:self.MSG_LEN]
                    buf = buf[self.MSG_LEN:]
                    port_idx = msg[0]
                    tris = struct.unpack_from('<I', msg, 1)[0]
                    lat  = struct.unpack_from('<I', msg, 5)[0]
                    port = struct.unpack_from('<I', msg, 9)[0]
                    self._q.put(('state', port_idx, tris, lat, port))

            sock.close()
            self._q.put(('disconnected',))

    # -- Pin widget -----------------------------------------------------------

    class PinWidget:
        """One row in the port frame: label, LED, optional toggle button."""

        def __init__(self, parent, row: int, port_letter: str, pin: int,
                     signal: str, qmp_client: QMPClient):
            self.port_letter = port_letter
            self.pin = pin
            self.qmp = qmp_client
            self.last_port_bit = 0

            pin_name = f'R{port_letter}{pin}'
            tk.Label(parent, text=pin_name, width=6, anchor='w',
                     font=('monospace', 10)).grid(row=row, column=0,
                                                   padx=(4, 2), sticky='w')
            tk.Label(parent, text=signal, width=18, anchor='w',
                     font=('monospace', 10)).grid(row=row, column=1,
                                                   padx=2, sticky='w')

            self.dir_label = tk.Label(parent, text='---', width=4,
                                      font=('monospace', 10, 'bold'))
            self.dir_label.grid(row=row, column=2, padx=2)

            self.canvas = tk.Canvas(parent, width=20, height=20,
                                    highlightthickness=0)
            self.led = self.canvas.create_oval(2, 2, 18, 18, fill=LED_OFF)
            self.canvas.grid(row=row, column=3, padx=4)

            self.toggle_btn = tk.Button(parent, text='Toggle', width=6,
                                        command=self._on_toggle)
            self.toggle_btn.grid(row=row, column=4, padx=4)
            self.toggle_btn.grid_remove()  # hidden by default

        def update(self, tris_bit: int, port_bit: int, lat_bit: int):
            self.last_port_bit = port_bit
            is_input = bool(tris_bit)
            direction = 'IN' if is_input else 'OUT'
            self.dir_label.config(text=direction,
                                  fg='#0066cc' if is_input else '#cc6600')
            # LED reflects PORT bit (shows actual pin level)
            color = LED_ON if port_bit else LED_OFF
            self.canvas.itemconfig(self.led, fill=color)
            # Toggle button only for input pins
            if is_input:
                self.toggle_btn.grid()
            else:
                self.toggle_btn.grid_remove()

        def _on_toggle(self):
            new_val = not bool(self.last_port_bit)
            try:
                path = gpio_path(self.port_letter)
                self.qmp.qom_set(path, f'pin{self.pin}', new_val)
            except (RuntimeError, ConnectionError):
                pass  # status bar will show error on next event

    # -- Port frame -----------------------------------------------------------

    class PortFrame(tk.LabelFrame):
        """One labeled frame per GPIO port containing its pin widgets."""

        def __init__(self, parent, port_letter: str, pins: list,
                     qmp_client: QMPClient):
            super().__init__(parent, text=f'  Port {port_letter}  ',
                             font=('sans-serif', 11, 'bold'), padx=4, pady=2)
            self.pin_widgets = {}

            # Header
            for col, hdr in enumerate(['Pin', 'Signal', 'Dir', 'State', '']):
                tk.Label(self, text=hdr, font=('sans-serif', 9, 'bold')
                         ).grid(row=0, column=col, padx=2)

            for i, (pin_num, signal, _dir_hint) in enumerate(pins):
                pw = PinWidget(self, i + 1, port_letter, pin_num, signal,
                               qmp_client)
                self.pin_widgets[pin_num] = pw

        def update_pins(self, tris: int, lat: int, port: int):
            for pin_num, pw in self.pin_widgets.items():
                pw.update((tris >> pin_num) & 1,
                          (port >> pin_num) & 1,
                          (lat >> pin_num) & 1)

    # -- Analog channel widget -------------------------------------------------

    class AnalogWidget:
        """One row in the analog frame: label, slider, value display."""

        def __init__(self, parent, row: int, channel: int, signal: str,
                     unit: str, qmp_client: QMPClient):
            self.channel = channel
            self.qmp = qmp_client

            tk.Label(parent, text=f'AN{channel}', width=6, anchor='w',
                     font=('monospace', 10)).grid(row=row, column=0,
                                                   padx=(4, 2), sticky='w')
            tk.Label(parent, text=signal, width=18, anchor='w',
                     font=('monospace', 10)).grid(row=row, column=1,
                                                   padx=2, sticky='w')

            self.slider_var = tk.IntVar(value=0)
            self.slider = tk.Scale(parent, from_=0, to=4095,
                                   orient='horizontal', length=200,
                                   variable=self.slider_var,
                                   showvalue=False,
                                   command=self._on_slide)
            self.slider.grid(row=row, column=2, padx=4)

            self.val_label = tk.Label(parent, text='0', width=6,
                                      font=('monospace', 10, 'bold'))
            self.val_label.grid(row=row, column=3, padx=2)

            tk.Label(parent, text=unit, width=4, anchor='w',
                     font=('monospace', 9)).grid(row=row, column=4,
                                                  padx=2, sticky='w')

        def _on_slide(self, _val_str):
            value = self.slider_var.get()
            self.val_label.config(text=str(value))
            try:
                self.qmp.qom_set(ADC_QOM_PATH, f'adc-ch{self.channel}', value)
            except (RuntimeError, ConnectionError):
                pass

        def set_value(self, value: int):
            self.slider_var.set(value)
            self.val_label.config(text=str(value))

    # -- Analog frame ---------------------------------------------------------

    class AnalogFrame(tk.LabelFrame):
        """Frame with sliders for VOLTU analog channels."""

        def __init__(self, parent, qmp_client: QMPClient):
            super().__init__(parent, text='  ADC Analog Inputs  ',
                             font=('sans-serif', 11, 'bold'), padx=4, pady=2)
            self.widgets = {}

            # Header
            for col, hdr in enumerate(['Ch', 'Signal', 'Value', '', 'Unit']):
                tk.Label(self, text=hdr, font=('sans-serif', 9, 'bold')
                         ).grid(row=0, column=col, padx=2)

            for i, ch in enumerate(sorted(VOLTU_ANALOG.keys())):
                sig, unit, _, _ = VOLTU_ANALOG[ch]
                aw = AnalogWidget(self, i + 1, ch, sig, unit, qmp_client)
                self.widgets[ch] = aw

        def read_initial(self, qmp_client: QMPClient):
            """Read current injection values from QOM."""
            for ch, aw in self.widgets.items():
                try:
                    val = qmp_client.qom_get(ADC_QOM_PATH, f'adc-ch{ch}')
                    aw.set_value(int(val))
                except (RuntimeError, ConnectionError):
                    pass

    # -- Main GUI app ---------------------------------------------------------

    class GpioGuiApp:
        QUEUE_CHECK_MS = 50

        def __init__(self, qmp_client: QMPClient, gpio_sock: str):
            self.qmp = qmp_client
            self.root = tk.Tk()
            self.root.title('PIC32MK GPIO Monitor')
            self.root.protocol('WM_DELETE_WINDOW', self._on_close)

            self.q = queue.Queue()
            self.port_frames = {}

            # Build frames for ports that have VOLTU pins
            ports_pins = {}  # port_letter -> [(pin, signal, dir), ...]
            for (pl, pn), (sig, dh) in sorted(VOLTU_PINS.items()):
                ports_pins.setdefault(pl, []).append((pn, sig, dh))

            container = tk.Frame(self.root)
            container.pack(fill='both', expand=True, padx=6, pady=4)

            for port_letter in sorted(ports_pins):
                pf = PortFrame(container, port_letter,
                               ports_pins[port_letter], qmp_client)
                pf.pack(fill='x', pady=2)
                self.port_frames[port_letter] = pf

            # Build analog frame if VOLTU_ANALOG channels are defined
            self.analog_frame = None
            if VOLTU_ANALOG:
                self.analog_frame = AnalogFrame(container, qmp_client)
                self.analog_frame.pack(fill='x', pady=2)

            # Status bar
            self.status_var = tk.StringVar(value='Connecting...')
            status_bar = tk.Label(self.root, textvariable=self.status_var,
                                  relief='sunken', anchor='w',
                                  font=('sans-serif', 9))
            status_bar.pack(fill='x', side='bottom')

            # Start socket listener
            self.listener = SocketListenerThread(gpio_sock, self.q)
            self.listener.start()

            # Do an initial QMP read so the GUI shows state before first event
            self._initial_read()

        def _initial_read(self):
            """Read current state of all ports and analog channels via QMP."""
            for port_letter, pf in self.port_frames.items():
                try:
                    path = gpio_path(port_letter)
                    tris = self.qmp.qom_get(path, 'tris-state')
                    lat  = self.qmp.qom_get(path, 'lat-state')
                    port = self.qmp.qom_get(path, 'port-state')
                    pf.update_pins(tris, lat, port)
                except (RuntimeError, ConnectionError):
                    pass
            if self.analog_frame:
                self.analog_frame.read_initial(self.qmp)
            self.status_var.set('Connected')

        def _check_queue(self):
            """Drain events from the listener thread and update widgets."""
            try:
                while True:
                    item = self.q.get_nowait()
                    if item[0] == 'state':
                        _, port_idx, tris, lat, port = item
                        pl = PORT_INDEX_TO_LETTER.get(port_idx)
                        if pl and pl in self.port_frames:
                            self.port_frames[pl].update_pins(tris, lat, port)
                        self.status_var.set('Connected (events streaming)')
                    elif item[0] == 'error':
                        self.status_var.set(f'Socket error: {item[1]}')
                    elif item[0] == 'disconnected':
                        self.status_var.set('Disconnected')
            except queue.Empty:
                pass
            self.root.after(self.QUEUE_CHECK_MS, self._check_queue)

        def run(self):
            self.root.after(self.QUEUE_CHECK_MS, self._check_queue)
            self.root.mainloop()

        def _on_close(self):
            self.listener.stop()
            self.root.destroy()

    # Actually launch the GUI
    app = GpioGuiApp(qmp, gpio_sock_path)
    app.run()


def main():
    parser = argparse.ArgumentParser(
        description='PIC32MK QEMU GPIO control via QMP/QOM')
    parser.add_argument('--qmp', default='/tmp/qemu-qmp.sock',
                        help='QMP socket path (unix) or tcp:host:port')
    sub = parser.add_subparsers(dest='command', required=True)

    p_set = sub.add_parser('set', help='Inject external input pin level')
    p_set.add_argument('port', help='Port letter (A–G)')
    p_set.add_argument('pin', type=int, help='Pin number (0–15)')
    p_set.add_argument('value', type=int, choices=[0, 1], help='Level: 0 or 1')

    p_get = sub.add_parser('get', help='Read current PORT pin value')
    p_get.add_argument('port', help='Port letter (A–G)')
    p_get.add_argument('pin', type=int, help='Pin number (0–15)')

    p_gp = sub.add_parser('get-port', help='Show full port state (LAT/PORT/TRIS)')
    p_gp.add_argument('port', help='Port letter (A–G)')

    sub.add_parser('list-paths', help='List QOM paths for GPIO devices')

    p_adc_set = sub.add_parser('adc-set', help='Inject analog value for ADC channel')
    p_adc_set.add_argument('channel', type=int, help='ADC channel number (0–53)')
    p_adc_set.add_argument('value', type=int, help='12-bit value (0–4095)')

    p_adc_get = sub.add_parser('adc-get', help='Read ADC channel conversion result')
    p_adc_get.add_argument('channel', type=int, help='ADC channel number (0–53)')

    sub.add_parser('adc-list', help='Show all mapped analog channel values')

    p_gui = sub.add_parser('gui', help='Launch tkinter GPIO visualization GUI')
    p_gui.add_argument('--gpio-sock', default='/tmp/gpio-events.sock',
                        help='GPIO chardev event socket (default: /tmp/gpio-events.sock)')

    args = parser.parse_args()

    # GUI mode needs QMP for input injection + chardev socket for events
    if args.command == 'gui':
        try:
            qmp = QMPClient(args.qmp)
        except (ConnectionRefusedError, FileNotFoundError) as e:
            print(f'Cannot connect to QMP socket {args.qmp!r}: {e}',
                  file=sys.stderr)
            sys.exit(1)
        try:
            cmd_gui(qmp, args.gpio_sock)
        finally:
            qmp.close()
        return

    try:
        qmp = QMPClient(args.qmp)
    except (ConnectionRefusedError, FileNotFoundError) as e:
        print(f'Cannot connect to QMP socket {args.qmp!r}: {e}', file=sys.stderr)
        sys.exit(1)

    try:
        if args.command == 'set':
            cmd_set(qmp, args.port, args.pin, args.value)
        elif args.command == 'get':
            cmd_get(qmp, args.port, args.pin)
        elif args.command == 'get-port':
            cmd_get_port(qmp, args.port)
        elif args.command == 'list-paths':
            cmd_list_paths(qmp)
        elif args.command == 'adc-set':
            cmd_adc_set(qmp, args.channel, args.value)
        elif args.command == 'adc-get':
            cmd_adc_get(qmp, args.channel)
        elif args.command == 'adc-list':
            cmd_adc_list(qmp)
    except RuntimeError as e:
        print(f'Error: {e}', file=sys.stderr)
        sys.exit(1)
    finally:
        qmp.close()


if __name__ == '__main__':
    main()
