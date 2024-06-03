from multiprocessing import Process
import os
import time
import client

def run_clients(num_clients, server_host, server_port, keys, log_dir):
    processes = []
    for i in range(num_clients):
        p = Process(target=client.client_task, args=(i, server_host, server_port, keys, log_dir))
        p.start()
        processes.append(p)
    
    for p in processes:
        p.join()

if __name__ == "__main__":
    server_host = "127.0.0.1"
    server_port = 12345
    keys = ["key1", "key2", "key3", "key4", "key5", "", " "]
    
    num_concurrent_clients = 100 
    log_dir = "./logs"
    os.makedirs(log_dir, exist_ok=True)

    run_clients(num_concurrent_clients, server_host, server_port, keys, log_dir)
