import socket
import random
import time
import sys
import os

def client_task(client_id, server_host, server_port, keys, log_dir):
    get_count = 0
    set_count = 0
    
    try:
        # Create a socket and connect to the server
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((server_host, server_port))
        
        for _ in range(10):
            key = random.choice(keys)
            if random.random() < 0.99:
                # Perform a get operation
                command = f"get {key}"
                get_count += 1
            else:
                # Perform a set operation with random value
                value = f"value{random.randint(1, 1000)}"
                command = f"set {key}={value}"
                set_count += 1
            
            # Send command to the server
            client_socket.sendall(command.encode())
            
            # Receive and print the response
            response = client_socket.recv(1024).decode()
            print(f"Response: {response}")
        
        client_socket.close()
    except (ConnectionRefusedError, socket.error):
        print("Connection failed. Reconnecting...")
        time.sleep(1)
        client_task(client_id, server_host, server_port, keys, log_dir)
    
    # Write the counts to a log file
    with open(os.path.join(log_dir, f"client_{client_id}.log"), 'w') as log_file:
        log_file.write(f"get_count: {get_count}\n")
        log_file.write(f"set_count: {set_count}\n")

if __name__ == "__main__":
    server_host = "127.0.0.1"
    server_port = 12345
    keys = ["key1", "key2", "key3"]
    log_dir = "./logs"
    os.makedirs(log_dir, exist_ok=True)
    
    client_id = int(sys.argv[1])  # Each client gets a unique ID passed as an argument
    client_task(client_id, server_host, server_port, keys, log_dir)
