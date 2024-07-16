import socket
import time
import threading


def send_fragmented_command(server_host, server_port, command_fragments):
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((server_host, server_port))
        for fragment in command_fragments:
            client_socket.sendall(fragment.encode())
            time.sleep(0.1)  # delay between fragments
        response = client_socket.recv(1024).decode()
        print(f"Response: {response.strip()}")
        client_socket.close()
    except (ConnectionRefusedError, socket.error) as e:
        print(f"Connection error: {e}")


def test_buffered_reading(server_host, server_port):
    commands = [
        ["get ", "key1\n"],
        ["set ", "key2=value", "123\n"],
        ["set ke", "y3=value", "456\n"],
        ["get ke", "y4\n"],
    ]

    threads = []
    for command_fragments in commands:
        thread = threading.Thread(
            target=send_fragmented_command,
            args=(server_host, server_port, command_fragments),
        )
        thread.start()
        threads.append(thread)

    for thread in threads:
        thread.join()


if __name__ == "__main__":
    server_host = "127.0.0.1"
    server_port = 12345

    test_buffered_reading(server_host, server_port)
