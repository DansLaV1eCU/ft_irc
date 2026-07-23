import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 6667))

# 1. Отправляем половину пароля (без \n)
s.sendall(b"PASS pa")
print("Отправлен кусок: PASS pa")
time.sleep(3) # Сервер должен молчать и копить это в буфере клиента

# 2. Доотправляем остаток пароля и команду NICK
s.sendall(b"s\r\nNICK testbot\r\n")
print("Отправлен остаток пароля и NICK")
time.sleep(1)

# 3. Отправляем USER для завершения регистрации
s.sendall(b"USER bot 0 * :Test Bot\r\n")
print("Отправлен USER. Ждем ответ сервера...")

# 4. Читаем ответ сервера (должен прийти MOTD)
print(s.recv(4096).decode('utf-8'))
s.close()