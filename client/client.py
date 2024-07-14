import socket
import random
import time
import sys
import os


def client_task(client_id, server_host, server_port, keys, log_dir):
    get_count = 0
    set_count = 0

    connected = False
    waiting_for_server = False
    client_socket = None

    while True:
        if not connected:
            if not waiting_for_server:
                print("waiting for server")
                waiting_for_server = True
            try:
                client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                client_socket.connect((server_host, server_port))
                print("connected to server")
                connected = True
            except (ConnectionRefusedError, socket.error):
                if connected:
                    print("Connection failed. Reconnecting...")
                connected = False
                time.sleep(1)
                continue

        try:
            for _ in range(10000):
                key = random.choice(keys)
                if random.random() < 0.99:
                    command = f"get {key}\n"
                    get_count += 1
                else:
                    value = f"value{random.randint(1, 1000)}"
                    command = f"set {key}={value}\n"
                    set_count += 1

                client_socket.sendall(command.encode())
                response = client_socket.recv(1024).decode()
                # print(f"{command}: {response}")

            client_socket.close()
            break
        except (ConnectionRefusedError, socket.error):
            print("Connection failed. Reconnecting...")
            connected = False
            time.sleep(1)
            continue

    with open(os.path.join(log_dir, f"client_{client_id}.log"), "w") as log_file:
        log_file.write(f"get_count: {get_count}\n")
        log_file.write(f"set_count: {set_count}\n")


if __name__ == "__main__":
    server_host = "127.0.0.1"
    server_port = 12345
    keys = ["key1", "key2", "key3", "key4", "key5", "key6"]
    log_dir = "./logs"
    os.makedirs(log_dir, exist_ok=True)

    client_id = int(sys.argv[1])
    client_task(client_id, server_host, server_port, keys, log_dir)
