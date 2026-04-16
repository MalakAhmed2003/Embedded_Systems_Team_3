from flask import Flask, render_template_string, request, jsonify
import requests
import threading

app = Flask(__name__)

# HC-10 port to connect to the arduino through it
BRIDGE_URL = "http://127.0.0.1:5001/send"
BRIDGE_STATUS_URL = "http://127.0.0.1:5001/status"

# Robot state representation in the website

_state_lock = threading.Lock()
_messages = []
_current_path = ""
_robot_started = False
_maze_solved = False
_path_length = 0
_expecting_path = False  


def update_state_from_arduino(msg: str):
    """Parse a message from the Arduino and update robot state."""
    global _robot_started, _maze_solved, _current_path, _path_length, _expecting_path

    with _state_lock:
        _messages.append(msg)

        if _expecting_path:
                  
            _current_path = msg
            _path_length = len(msg)
            _expecting_path = False
            return

        if "=== OPTIMIZED GLOBAL PATH ===" in msg:
            _expecting_path = True

        elif msg.startswith("Raw Path:"):
            raw = msg[len("Raw Path:"):].strip()
            _current_path = raw
            _path_length = len(raw)

        elif "=== MAZE SOLVED ===" in msg:
            _maze_solved = True
            _robot_started = False

        elif "=== OPTIMIZED MAZE SOLVED! ===" in msg:
            _maze_solved = True

        elif "Local Action:" in msg:
            _robot_started = True

        elif "Robot Started" in msg:
            _robot_started = False
            _maze_solved = False
            _current_path = ""
            _path_length = 0

# HTML code for the interface of the website (Simple interface)

HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>Robot Controller - Bluetooth</title>
    <style>
        body {
            font-family: 'Courier New', monospace;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: #1a1a1a;
            color: #00ff00;
        }
        .container {
            background: #0a0a0a;
            padding: 20px;
            border-radius: 10px;
            border: 1px solid #00ff00;
        }
        h1 { text-align: center; color: #00ff00; margin-top: 0; }
        .bluetooth-status {
            text-align: center;
            padding: 5px;
            margin-bottom: 10px;
            border-radius: 5px;
            font-size: 12px;
        }
        .connected    { background: #004400; color: #00ff00; }
        .disconnected { background: #440000; color: #ff0000; }
        .buttons {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
            margin: 20px 0;
        }
        button {
            background: #00ff00;
            color: black;
            border: none;
            padding: 12px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            font-family: monospace;
            border-radius: 5px;
        }
        button:hover { background: #00cc00; transform: scale(1.02); }
        .status-box, .path-box {
            background: black;
            padding: 15px;
            border-radius: 5px;
            margin: 20px 0;
            border: 1px solid #00ff00;
        }
        .path-box { word-break: break-all; font-size: 16px; font-weight: bold; }
        .log {
            background: black;
            padding: 10px;
            height: 400px;
            overflow-y: auto;
            font-family: monospace;
            font-size: 12px;
            border-radius: 5px;
            border: 1px solid #00ff00;
        }
        .log-line {
            padding: 2px 0;
            border-bottom: 1px solid #1a1a1a;
            white-space: pre-wrap;
        }
        .status-label { color: #00ff00; font-weight: bold; }
        .command-info { font-size: 12px; text-align: center; margin-top: 10px; color: #888; }
    </style>
</head>
<body>
<div class="container">
    <h1>🤖 ROBOT CONTROLLER (Bluetooth)</h1>

    <div id="btStatus" class="bluetooth-status disconnected">
        🔵 HC-10 Bridge: Checking...
    </div>

    <div class="buttons">
        <button onclick="sendCmd('start')">🚀 START EXPLORATION</button>
        <button onclick="sendCmd('optimized')">⚡ RUN OPTIMIZED</button>
        <button onclick="sendCmd('optimize')">📊 SHOW OPTIMIZED PATH</button>
        <button onclick="sendCmd('stop')">⏹ STOP</button>
    </div>

    <div class="status-box">
        <span class="status-label">Exploration Started:</span> <span id="started">No</span><br>
        <span class="status-label">Maze Solved:</span>         <span id="solved">No</span><br>
        <span class="status-label">Path Length:</span>         <span id="pathLength">0</span>
    </div>

    <div class="path-box">
        <span class="status-label">Current Path:</span><br>
        <span id="path">(empty)</span>
    </div>

    <div class="log" id="log">
        <div class="log-line">> Ready. Controlling robot via Bluetooth HC-10</div>
        <div class="log-line">> Commands: start | optimized | optimize | stop</div>
    </div>
    <div class="command-info">
        💡 start = begin exploration &nbsp;|&nbsp;
        optimized = run optimized path &nbsp;|&nbsp;
        optimize = show optimized path &nbsp;|&nbsp;
        stop = stop motors
    </div>
</div>

<script>
    function addLog(msg) {
        const log = document.getElementById('log');
        const time = new Date().toLocaleTimeString();
        log.innerHTML += `<div class="log-line">[${time}] ${msg}</div>`;
        log.scrollTop = log.scrollHeight;
        while (log.children.length > 200) log.removeChild(log.firstChild);
    }

    function sendCmd(cmd) {
        addLog('> Sending: ' + cmd);
        fetch('/command', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({cmd: cmd})
        })
        .then(r => r.json())
        .then(data => {
            if (data.error)   addLog('! Error: ' + data.error);
            else if (data.ok) addLog('✓ Command sent via Bluetooth');
        })
        .catch(err => addLog('! Fetch error: ' + err));
    }

    function updateStatus() {
        fetch('/status')
        .then(r => r.json())
        .then(data => {
            document.getElementById('started').innerText    = data.started    ? 'Yes' : 'No';
            document.getElementById('solved').innerText     = data.solved     ? 'Yes' : 'No';
            document.getElementById('pathLength').innerText = data.path_length || '0';
            document.getElementById('path').innerText       = data.path       || '(empty)';

            const btDiv = document.getElementById('btStatus');
            if (data.bridge_connected) {
                btDiv.innerText   = '🟢 HC-10 Bridge: Connected';
                btDiv.className   = 'bluetooth-status connected';
            } else {
                btDiv.innerText   = '🔴 HC-10 Bridge: Disconnected — run hc10_bridge.py first';
                btDiv.className   = 'bluetooth-status disconnected';
            }

            if (data.new_messages && data.new_messages.length > 0) {
                data.new_messages.forEach(m => addLog(m));
            }
        })
        .catch(err => console.log('Status error:', err));
    }

    setInterval(updateStatus, 500);
    updateStatus();
</script>
</body>
</html>
"""

# Web server backend that routes commands between the website and
# the bluetooth module through the bridge code

@app.route('/')
def index():
    return render_template_string(HTML)


@app.route('/command', methods=['POST'])
def command():
    data = request.json or {}
    cmd = data.get('cmd', '').strip()
    if not cmd:
        return jsonify({'error': 'No command provided'})

    try:
        response = requests.post(BRIDGE_URL, json={"cmd": cmd}, timeout=2)
        result = response.json()
        if result.get('status') == 'ok':
            with _state_lock:
                _messages.append(f"Sent: {cmd}")
            return jsonify({'ok': True})
        else:
            err = result.get('error', 'Bridge error')
            with _state_lock:
                _messages.append(f"Error: {err}")
            return jsonify({'error': err})

    except requests.exceptions.ConnectionError:
        msg = "HC-10 bridge not running — start hc10_bridge.py first"
        with _state_lock:
            _messages.append(msg)
        return jsonify({'error': msg})
    except Exception as e:
        with _state_lock:
            _messages.append(f"Failed: {e}")
        return jsonify({'error': str(e)})


@app.route('/status')
def status():
    # Check if the bridge is connected
    bridge_connected = False
    try:
        r = requests.get(BRIDGE_STATUS_URL, timeout=1)
        bridge_connected = r.status_code == 200
    except Exception:
        bridge_connected = False

    with _state_lock:
        new_msgs = list(_messages)
        _messages.clear()
        return jsonify({
            'started':          _robot_started,
            'solved':           _maze_solved,
            'path':             _current_path,
            'path_length':      _path_length,
            'new_messages':     new_msgs,
            'bridge_connected': bridge_connected,
        })


@app.route('/from-arduino', methods=['POST'])
def from_arduino():
    """Receives data forwarded by hc10_bridge.py and updates robot state."""
    data = request.json or {}
    msg = data.get('data', '').strip()
    if msg:
        print(f"[Arduino] {msg}")
        update_state_from_arduino(msg)
    return jsonify({'status': 'ok'})


if __name__ == '__main__':
    print("\n" + "=" * 60)
    print("  🤖 ROBOT CONTROLLER - BLUETOOTH VERSION")
    print("=" * 60)
    print("  Run hc10_bridge.py first, then open http://127.0.0.1:5000")
    print("=" * 60 + "\n")
    app.run(host='127.0.0.1', port=5000, debug=False, use_reloader=False)
