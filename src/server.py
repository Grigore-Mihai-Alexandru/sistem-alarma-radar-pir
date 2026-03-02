from flask import Flask, request, jsonify, render_template_string, redirect, url_for
import time, os

app = Flask(__name__)
UPLOAD_FOLDER = 'capturi'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

state = {"armed": False, "last_image": None}

HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Alarma Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Segoe UI', sans-serif; text-align: center; background: #1a1a1a; color: #eee; margin: 0; padding: 20px; }
        .container { max-width: 600px; margin: auto; background: #2d2d2d; padding: 20px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }
        .status-box { padding: 20px; margin: 20px 0; border-radius: 10px; font-size: 22px; font-weight: bold; text-transform: uppercase; }
        .armed { background: #c62828; border: 2px solid #ff5252; color: white; }
        .disarmed { background: #2e7d32; border: 2px solid #69f0ae; color: white; }
        .btn { padding: 15px 40px; font-size: 18px; cursor: pointer; border: none; border-radius: 50px; background: #2196F3; color: white; transition: 0.3s; box-shadow: 0 4px 15px rgba(33,150,243,0.3); }
        .btn:hover { background: #1976D2; transform: translateY(-2px); }
        .btn:active { transform: translateY(1px); }
        img { max-width: 100%; border-radius: 10px; border: 2px solid #444; margin-top: 20px; }
        .footer { margin-top: 20px; font-size: 12px; color: #888; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Control Alarma ESP32</h1>
        
        <div class="status-box {{ 'armed' if armed else 'disarmed' }}">
            Sistemul este: {{ 'ARMAT' if armed else 'DEZARMAT' }}
        </div>
        
        <form action="/toggle" method="POST">
            <button type="submit" class="btn">
                {{ 'DEZARMEAZĂ ACUM' if armed else 'ARMEAZĂ ACUM' }}
            </button>
        </form>
        
        <hr style="margin: 30px 0; border: 0; border-top: 1px solid #444;">
        
        <h3>Ultima detecție (Cameră)</h3>
        {% if last_image %}
            <p style="font-size: 14px; color: #aaa;">Fișier: {{ last_image }}</p>
            <img src="/capturi/{{ last_image }}">
        {% else %}
            <p style="color: #666;">Nicio activitate detectată momentan.</p>
        {% endif %}
        
        <div class="footer">
            Server IP: {{ server_ip }} | Port: 8000
        </div>
    </div>
</body>
</html>
"""

@app.route('/')
def index():
    # obtinem ip-ul serverului pentru a afisa in pagina
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try: s.connect(('8.8.8.8', 1)); server_ip = s.getsockname()[0]
    except: server_ip = "127.0.0.1"
    finally: s.close()

    return render_template_string(HTML_PAGE, 
                                 armed=state["armed"], 
                                 last_image=state["last_image"],
                                 server_ip=server_ip)

@app.route('/toggle', methods=['POST'])
def toggle():
    state["armed"] = not state["armed"]
    print(f"Stare sistem schimbată în: {'ARMAT' if state['armed'] else 'DEZARMAT'}")
    
    # redirectionare pentru a nu da toggle la refresh
    return redirect(url_for('index'))

@app.route('/status')
def get_status():
    return jsonify({"armed": state["armed"]})

@app.route('/upload', methods=['POST'])
def upload():
    timestamp = time.strftime("%Y__%m__%d__%H__%M__%S")
    filename = f"intruder__{timestamp}.jpg"
    
    cale_fisier = os.path.join(UPLOAD_FOLDER, filename)
    with open(cale_fisier, "wb") as f:
        f.write(request.data)
        
    state["last_image"] = filename
    print(f"Alerta! Imagine salvata: {filename}")
    return "OK"

@app.route('/capturi/<path:filename>')
def get_image(filename):
    from flask import send_from_directory
    return send_from_directory(UPLOAD_FOLDER, filename)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000)