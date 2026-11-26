#include "one-xfs-sync/epoll_manager/epoll_manager.h"

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

// Вспомогательная функция для получения реального порта, если передан 0
static uint16_t get_bound_port(int sock_fd) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int res = getsockname(sock_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    assert(res == 0);
    return ntohs(addr.sin_port);
}

// Вспомогательная функция для создания блокирующего или неблокирующего клиента
static int connect_client(uint16_t port, bool nonblocking = false) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int res = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    assert(res == 0);

    if (nonblocking) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return fd;
}

// Тест жизненного цикла
void test_lifecycle() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);

    net_loop_destroy(loop);
    net_loop_destroy(nullptr);
    std::cout << "[PASS] test_lifecycle\n";
}

// Тест таймаутов и пустого ожидания
void test_wait_timeouts() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);

    net_event_t events[4];

    int n = net_loop_wait(loop, events, 4, 0);
    assert(n == 0);

    auto start = std::chrono::steady_clock::now();
    n = net_loop_wait(loop, events, 4, 20);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();

    assert(n == 0);
    assert(elapsed >= 15);

    net_loop_destroy(loop);
    std::cout << "[PASS] test_wait_timeouts\n";
}

// Тест потокобезопасного пробуждения (wakeup)
void test_wakeup() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);

    std::thread waker([loop]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        net_loop_wakeup(loop);
    });

    net_event_t events[4];
    // Бесконечное ожидание должно прерваться сигналом wakeup
    int n = net_loop_wait(loop, events, 4, -1);
    assert(n == 1);
    assert(events[0].event_type == NET_EVENT_WAKEUP);
    assert(events[0].fd == -1);

    waker.join();

    // Проверяем, что счётчик eventfd сброшен и повторный wait не срабатывает вхолостую
    n = net_loop_wait(loop, events, 4, 0);
    assert(n == 0);

    net_loop_destroy(loop);
    std::cout << "[PASS] test_wakeup\n";
}

// Тест регистрации и удаления внешних дескрипторов (add_fd / remove_fd)
void test_external_fd() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);

    int pipe_fds[2];
    assert(pipe(pipe_fds) == 0);

    // Регистрируем читающий конец pipe
    assert(net_loop_add_fd(loop, pipe_fds[0]) == true);

    // Повторная регистрация того же fd должна вернуть false
    assert(net_loop_add_fd(loop, pipe_fds[0]) == false);

    // Пишем байт в pipe
    const char msg = 'X';
    assert(write(pipe_fds[1], &msg, 1) == 1);

    net_event_t events[4];
    int n = net_loop_wait(loop, events, 4, 100);
    assert(n == 1);
    assert(events[0].event_type == NET_EVENT_READ);
    assert(events[0].fd == pipe_fds[0]);

    // Удаляем fd из цикла
    assert(net_loop_remove_fd(loop, pipe_fds[0]) == true);
    // Повторное удаление должно вернуть false
    assert(net_loop_remove_fd(loop, pipe_fds[0]) == false);

    // Убеждаемся, что непрочитанные данные в pipe больше не генерируют событий в loop
    n = net_loop_wait(loop, events, 4, 0);
    assert(n == 0);

    // remove_fd не закрывает сам fd, мы можем прочитать из него
    char buf = 0;
    assert(read(pipe_fds[0], &buf, 1) == 1);
    assert(buf == 'X');

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    net_loop_destroy(loop);
    std::cout << "[PASS] test_external_fd\n";
}

// Тест базового TCP-обмена: listen -> accept -> send -> recv
void test_tcp_echo_flow() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);

    // Порт 0 позволяет ядру выделить свободный порт
    int listen_fd = net_loop_create_listen_socket(loop, 0);
    assert(listen_fd >= 0);
    uint16_t port = get_bound_port(listen_fd);

    // Пока клиентов нет, accept в неблокирующем режиме должен вернуть -1
    assert(net_loop_accept(loop, listen_fd) == -1);

    // Подключаем клиента
    int client_fd = connect_client(port);

    // Ожидаем событие нового подключения на listen_fd
    net_event_t events[4];
    int n = net_loop_wait(loop, events, 4, 100);
    assert(n == 1);
    assert(events[0].event_type == NET_EVENT_READ);
    assert(events[0].fd == listen_fd);

    // Принимаем соединение
    int server_conn_fd = net_loop_accept(loop, listen_fd);
    assert(server_conn_fd >= 0);

    // Отправляем данные от клиента на сервер
    const char* ping = "Hello, net_loop!";
    size_t ping_len = std::strlen(ping);
    assert(send(client_fd, ping, ping_len, 0) == static_cast<ssize_t>(ping_len));

    // Ожидаем NET_EVENT_READ на принятом серверном сокете
    n = net_loop_wait(loop, events, 4, 100);
    assert(n == 1);
    assert(events[0].event_type == NET_EVENT_READ);
    assert(events[0].fd == server_conn_fd);

    // Читаем данные через net_loop_try_recv
    char recv_buf[64] = {0};
    ssize_t r = net_loop_try_recv(server_conn_fd, recv_buf, sizeof(recv_buf));
    assert(r == static_cast<ssize_t>(ping_len));
    assert(std::strcmp(recv_buf, ping) == 0);

    // Повторное чтение при пустом буфере должно вернуть 0
    assert(net_loop_try_recv(server_conn_fd, recv_buf, sizeof(recv_buf)) == 0);

    // Отправляем ответ через net_loop_try_send
    const char* pong = "Pong!";
    size_t pong_len = std::strlen(pong);
    ssize_t s = net_loop_try_send(loop, server_conn_fd, pong, pong_len);
    assert(s == static_cast<ssize_t>(pong_len));

    // При полной отправке EPOLLOUT должен быть деактивирован
    n = net_loop_wait(loop, events, 4, 10);
    assert(n == 0);

    // Проверяем получение на клиенте
    char client_buf[64] = {0};
    assert(recv(client_fd, client_buf, sizeof(client_buf), 0) == static_cast<ssize_t>(pong_len));
    assert(std::strcmp(client_buf, pong) == 0);

    close(client_fd);
    close(server_conn_fd);
    close(listen_fd);
    net_loop_destroy(loop);
    std::cout << "[PASS] test_tcp_echo_flow\n";
}

// Тест управления EPOLLOUT при переполнении буфера сокета (EAGAIN)
void test_epollout_backpressure() {
    net_loop_t* loop = net_loop_create();
    int listen_fd = net_loop_create_listen_socket(loop, 0);
    uint16_t port = get_bound_port(listen_fd);

    int client_fd = connect_client(port);

    net_event_t events[4];
    assert(net_loop_wait(loop, events, 4, 100) == 1);
    int server_conn_fd = net_loop_accept(loop, listen_fd);
    assert(server_conn_fd >= 0);

    // Уменьшаем буфер отправки
    int sndbuf = 16384;
    setsockopt(server_conn_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    // Пытаемся отправить большой блок данных (4 МБ), пока клиент ничего не читает
    std::vector<char> big_data(4 * 1024 * 1024, 'A');
    size_t total_sent = 0;
    bool hit_eagain = false;

    while (total_sent < big_data.size()) {
        ssize_t sent = net_loop_try_send(
            loop, server_conn_fd, big_data.data() + total_sent, big_data.size() - total_sent);
        assert(sent >= 0);
        if (sent == 0) {
            hit_eagain = true;
            break;
        }
        total_sent += static_cast<size_t>(sent);
    }

    assert(hit_eagain == true);
    assert(total_sent > 0);

    // Клиент вычитывает данные из сокета, освобождая буфер ядра на сервере
    std::vector<char> drain_buf(65536);
    size_t drained = 0;
    while (drained < total_sent) {
        ssize_t r = recv(client_fd, drain_buf.data(), drain_buf.size(), 0);
        assert(r > 0);
        drained += static_cast<size_t>(r);
    }

    // Теперь цикл должен сообщить о готовности к записи (NET_EVENT_WRITE)
    int n = net_loop_wait(loop, events, 4, 100);
    assert(n == 1);
    assert(events[0].fd == server_conn_fd && events[0].event_type == NET_EVENT_WRITE);

    // Отправляем небольшой остаток, который поместится целиком
    const char* tail = "END";
    assert(net_loop_try_send(loop, server_conn_fd, tail, 3) == 3);

    // После полной передачи EPOLLOUT должен автоматически выключиться
    n = net_loop_wait(loop, events, 4, 10);
    assert(n == 0);

    close(client_fd);
    close(server_conn_fd);
    close(listen_fd);
    net_loop_destroy(loop);
    std::cout << "[PASS] test_epollout_backpressure\n";
}

// Тест разрыва соединения
void test_disconnect_and_error() {
    net_loop_t* loop = net_loop_create();
    int listen_fd = net_loop_create_listen_socket(loop, 0);
    uint16_t port = get_bound_port(listen_fd);

    int client_fd = connect_client(port);

    net_event_t events[4];
    assert(net_loop_wait(loop, events, 4, 100) == 1);
    int server_conn_fd = net_loop_accept(loop, listen_fd);
    assert(server_conn_fd >= 0);

    // Принудительный сброс соединения
    linger lg{1, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    close(client_fd);
    int n = net_loop_wait(loop, events, 4, 100);
    assert(n == 1);
    assert(events[0].fd == server_conn_fd);
    // epoll генерирует EPOLLERR | EPOLLHUP, что маппится в NET_EVENT_ERROR
    assert(events[0].event_type == NET_EVENT_ERROR);
    // Попытка чтения из разорванного сокета должна вернуть -1
    char buf[16];
    assert(net_loop_try_recv(server_conn_fd, buf, sizeof(buf)) == -1);
    close(server_conn_fd);
    close(listen_fd);
    net_loop_destroy(loop);
    std::cout << "[PASS] test_disconnect_and_error\n";
}

int main() {
    test_lifecycle();
    test_wait_timeouts();
    test_wakeup();
    test_external_fd();
    test_tcp_echo_flow();
    test_epollout_backpressure();
    test_disconnect_and_error();
    std::cout << "\nAll unit tests passed successfully!\n";
    return 0;
}