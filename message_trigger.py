import socket
import json
import time
import random

def send_message(msg_type, msg_text, clientId):
    msg = {"type": msg_type, "message": msg_text, "client": clientId, "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")}
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect(("127.0.0.1", 5050))
            s.send(json.dumps(msg).encode("utf-8"))
        except Exception as e:
            print("Error sending:", e)

start_time = time.time()
sent_start_message = {1: False, 2: False, 3: False}

clients = [1, 2, 3]
messages = [
    {"type": "INFO", "message": "This is random information!"},
    {"type": "WARNING", "message": "This is random warning!"},
    {"type": "ERROR", "message": "This is random error!"},
    {"type": "CRITICAL", "message": "This is a CRITICAL error, process will be stopped!!"}
]

while True:
    client_id = random.choice(clients)
    if not sent_start_message[client_id]:
        start_time = time.time()
        # send a specific first message that the client (simulated module) started
        send_message("INFO", "Module started", client_id)
        sent_start_message[client_id] = True
        time.sleep(0.2)
        continue

    elapsed = time.time() - start_time

    # Block CRITICAL messages for about the first 10 seconds (can be more because of the reset of start time inside the previous if block)
    if elapsed < 10:
        allowed_messages = [m for m in messages if m["type"] != "CRITICAL"]
    else:
        allowed_messages = messages

    msg = random.choice(allowed_messages)
    send_message(msg["type"], msg["message"], client_id)
    time.sleep(random.uniform(0.05, 0.15))  # 50 to 150 ms delay

