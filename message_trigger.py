import socket
import json
import time
import random
import argparse
import threading
import signal
import sys

"""
Simulates three modules sending status and sensor data over TCP.

Each module runs in its own thread and sends messages like INFO, WARNING, ERROR, or CRITICAL
to a central server. Useful for testing and debugging the EventMonitor application.
"""

def signal_handler(sig, frame):
    print("Termination signal received. Cleaning up.")
    sys.exit(0)

signal.signal(signal.SIGTERM, signal_handler)

# Constant numbers
SOCKET_TIMEOUT_SEC = 0.4
NORMAL_SAMPLE_WINDOW = 5
MIN_MESSAGE_INTERVAL_SEC = 0.05
MAX_MESSAGE_INTERVAL_SEC = 0.15

# Parse arguements for tcp connection
parser = argparse.ArgumentParser(description="TCP message generator.")
parser.add_argument("--ip", type=str, default="127.0.0.1", help="Target IP address")
parser.add_argument("--port", type=int, default=1024, help="Target port")
args = parser.parse_args()

# Define value ranges and weights
ranges = [
    {"range": [(0, 5), (95, 100)], "type": "CRITICAL", "weight": 0.001},
    {"range": [(5.1, 15), (85, 94.9)], "type": "ERROR", "weight": 0.009},
    {"range": [(15.1, 25), (75, 84.9)], "type": "WARNING", "weight": 0.4},
    {"range": [(25.1, 74.9)], "type": "INFO", "weight": 0.95},  # normal
]
weights = [r["weight"] for r in ranges]

# State per client variables
block_module = {1: False, 2: False, 3: False}
clients = [1, 2, 3]
sockets = {}
last_logged_range_per_client = {1: None, 2: None, 3: None}
normal_counter = {1: 0, 2: 0, 3: 0}
normal_values = {1: [], 2: [], 3: []}
active_clients = {1: True, 2: True, 3: True}
lock = threading.Lock()

def connect_client(client_id):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((args.ip, args.port))
        s.settimeout(SOCKET_TIMEOUT_SEC)
        with lock:
            sockets[client_id] = s
        return True
    except Exception as e:
        print(f"[Client {client_id}] Connection error:", e)
        return False

def send_message(client_id, msg_type, msg_text):
    if block_module.get(client_id, False):
        return
    msg = {
            "type": msg_type,
            "message": msg_text,
            "client": client_id,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }

    message_str = (json.dumps(msg) + '\n').encode("utf-8")

    with lock:
        sock = sockets.get(client_id)
        if not sock:
            return
        try:
            sock.send(message_str)
        except Exception as e:
            print(f"[Client {client_id}] Send failed:", e)
            try:
                sock.close()
            except:
                pass
            sockets.pop(client_id, None)
            active_clients[client_id] = False

def client_loop(client_id):
    if not connect_client(client_id):
        return

    send_message(client_id, "INFO", "Module started")

    try:
        while True:
            with lock:
                if not active_clients.get(client_id, False):
                    break
            selected_range = random.choices(ranges, weights=weights, k=1)[0]
            subrange = random.choice(selected_range["range"])
            value = random.uniform(subrange[0], subrange[1])
            msg_text = f"value:{value:.2f}"

            send_message(client_id, "DATA", msg_text)

            range_label = selected_range["type"] if selected_range["type"] != "INFO" else "NORMAL"
            last_logged_range = last_logged_range_per_client[client_id]

            if range_label != last_logged_range:
                if range_label != "NORMAL":
                    log_text = f"Value entered {range_label} range: {value:.2f}"
                    send_message(client_id, range_label, log_text)
                else:
                    log_text = f"Value back to normal: {value:.2f}"
                    send_message(client_id, "INFO", log_text)

                last_logged_range_per_client[client_id] = range_label

            if range_label == "NORMAL":
                normal_counter[client_id] += 1
                normal_values[client_id].append(value)
                if normal_counter[client_id] == NORMAL_SAMPLE_WINDOW:
                    avg_value = sum(normal_values[client_id]) / NORMAL_SAMPLE_WINDOW
                    log_text = f"Value stable for last 5 samples, average value: {avg_value:.2f}"
                    send_message(client_id, "INFO", log_text)
                    normal_counter[client_id] = 0
                    normal_values[client_id] = []
            else:
                normal_counter[client_id] = 0
                normal_values[client_id] = []

            if range_label == "CRITICAL":
                print(f"[Client {client_id}] CRITICAL value sent. Closing socket.")
                with lock:
                    if client_id == 3:
                        # shutdown all modules when a critical error for module 3 occurs
                        for id in active_clients:
                            block_module[id] = True

                        # application will stop in Qt side, give sometime to close the sockets there before do it here
                        time.sleep(5)
                        for id in active_clients:
                            active_clients[id] = False
                    else:
                        active_clients[client_id] = False
                break

            # Introduce randomized delay to simulate realistic message timing in a data pipeline
            time.sleep(random.uniform(MIN_MESSAGE_INTERVAL_SEC, MAX_MESSAGE_INTERVAL_SEC))

    finally:
        with lock:
            sock = sockets.pop(client_id, None)
            if sock:
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except:
                    pass
                    sock.close()

# Launch all client threads
threads = []
for client_id in clients:
    t = threading.Thread(target=client_loop, args=(client_id,), daemon=True)
    t.start()
    threads.append(t)

# Wait until all clients are finished
try:
    for t in threads:
        t.join()
except Exception as e:
    logging.error(f"Thread join failed: {e}")
finally:
    sys.exit(0)
