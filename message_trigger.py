import socket
import json
import time
import random
import argparse

# Parse arguements for tcp connection
parser = argparse.ArgumentParser(description="TCP message generator.")
parser.add_argument("--ip", type=str, default="127.0.0.1", help="Target IP address")
parser.add_argument("--port", type=int, default=5050, help="Target port")
args = parser.parse_args()

def send_message(msg_type, msg_text, clientId):
    msg = {"type": msg_type, "message": msg_text, "client": clientId, "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")}
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect((args.ip, args.port))
            s.send(json.dumps(msg).encode("utf-8"))
        except Exception as e:
            print("Error sending {args.ip}:{args.port}:", e)

sent_start_message = {1: False, 2: False, 3: False}
last_logged_range_per_client = {1: None, 2: None, 3: None}
normal_counter = {1: 0, 2: 0, 3: 0}
normal_values = {1: [], 2: [], 3: []}
stopped_clients = {1: False, 2: False, 3: False}
clients = [1, 2, 3]

# Define value ranges and weights
ranges = [
    {"range": [(0, 5), (95, 100)], "type": "CRITICAL", "weight": 0.02},
    {"range": [(5.1, 15), (85, 94.9)], "type": "ERROR", "weight": 0.98},
    {"range": [(15.1, 25), (75, 84.9)], "type": "WARNING", "weight": 0.02},
    {"range": [(25.1, 74.9)], "type": "INFO", "weight": 0.70},  # normal
]
weights = [r["weight"] for r in ranges]

# Main loop
while True:
    client_id = random.choice(clients)
    if stopped_clients[client_id]:
        time.sleep(0.05)
        continue

    if not sent_start_message[client_id]:
        # send a specific first message that the client (simulated module) started
        send_message("INFO", "Module started", client_id)
        sent_start_message[client_id] = True
        time.sleep(0.2)
        continue

    selected_range = random.choices(ranges, weights=weights, k=1)[0]
    subrange = random.choice(selected_range["range"])
    value = random.uniform(subrange[0], subrange[1])
    msg_text = f"value:{value:.2f}"

    # Always send DATA message
    send_message("DATA", msg_text, client_id)

    # Determine current logical range label
    range_label = selected_range["type"] if selected_range["type"] != "INFO" else "NORMAL"

    # Check for transition → send log if needed
    last_logged_range = last_logged_range_per_client[client_id]

    if range_label != last_logged_range:
        if range_label != "NORMAL":
            log_text = f"Value entered {range_label} range: {value:.2f}"
            send_message(range_label, log_text, client_id)
        else:
            # Back to normal → send INFO
            log_text = f"Value back to normal: {value:.2f}"
            send_message("INFO", log_text, client_id)

        # Update last logged state for this client
        last_logged_range_per_client[client_id] = range_label

        # Also send an INFO log message if 5 valeus in a row per client are within the NORMAL range
        if range_label == "NORMAL":
            normal_counter[client_id] += 1
            normal_values[client_id].append(value)

            if normal_counter[client_id] == 5:
                avg_value = sum(normal_values[client_id]) / 5
                log_text = f"Value stable for last 5 samples, average value: {avg_value:.2f}"
                send_message("INFO", log_text, client_id)

                # Reset counter and values so we calculate the next 5
                normal_counter[client_id] = 0
                normal_values[client_id] = []

        else:
            # if the value isn't NORMAL reset the counter and the saved values if there is something there
            if normal_counter[client_id] > 0:
                normal_counter[client_id] = 0
                normal_values[client_id] = []

        if range_label == "CRITICAL":
             stopped_clients[client_id] = True

    time.sleep(random.uniform(0.05, 0.15))  # 50 to 150 ms delay

