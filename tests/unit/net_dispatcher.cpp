#include "one-xfs-sync/utils/net_dispatcher.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <numeric>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// Вспомогательная структура для пары соединённых локальных TCP-сокетов
struct TcpPair {
    int listen_fd{-1};
    int server_fd{-1}; // Неблокирующий сокет, зарегистрированный в net_loop
    int client_fd{-1}; // Блокирующий сокет для чтения на принимающей стороне

    static TcpPair create(net_loop_t* loop) {
        TcpPair p;
        p.listen_fd = net_loop_create_listen_socket(loop, 0);
        assert(p.listen_fd >= 0);

        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        assert(getsockname(p.listen_fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0);

        p.client_fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(p.client_fd >= 0);
        assert(connect(p.client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

        net_event_t ev{};
        assert(net_loop_wait(loop, &ev, 1, 100) == 1);
        assert(ev.fd == p.listen_fd);

        p.server_fd = net_loop_accept(loop, p.listen_fd);
        assert(p.server_fd >= 0);
        return p;
    }

    void close_all() {
        if (client_fd >= 0) close(client_fd);
        if (server_fd >= 0) close(server_fd);
        if (listen_fd >= 0) close(listen_fd);
    }
};

// Вспомогательная функция гарантированного вычитывания точного числа байт на клиенте
static std::vector<uint8_t> recv_exact(int fd, size_t expected_bytes) {
    std::vector<uint8_t> result(expected_bytes);
    size_t total = 0;
    while (total < expected_bytes) {
        ssize_t r = recv(fd, result.data() + total, expected_bytes - total, 0);
        assert(r > 0);
        total += static_cast<size_t>(r);
    }
    return result;
}

// Тест мгновенной отправки без буферизации
void test_immediate_send() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);
    TcpPair pair = TcpPair::create(loop);

    net_dispatcher dispatcher(loop);
    assert(dispatcher.has_pending(pair.server_fd) == false);

    const std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', '!'};
    bool ok = dispatcher.send(pair.server_fd, payload);
    assert(ok == true);

    // Данные должны уйти сразу, очередь должна остаться пустой
    assert(dispatcher.has_pending(pair.server_fd) == false);
    net_event_t ev{};
    assert(net_loop_wait(loop, &ev, 1, 10) == 0);

    // Проверяем доставку на клиенте
    auto received = recv_exact(pair.client_fd, payload.size());
    assert(received == payload);

    pair.close_all();
    net_loop_destroy(loop);
    std::cout << "[PASS] test_immediate_send\n";
}

// Тест частичной записи и доотправки через on_writable
void test_partial_write_and_flush() {
    net_loop_t* loop = net_loop_create();
    TcpPair pair = TcpPair::create(loop);

    // Уменьшаем буфер отправки ядра, чтобы гарантированно вызвать частичную запись
    int sndbuf = 16384;
    setsockopt(pair.server_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    net_dispatcher dispatcher(loop);

    // Создаём большой объём данных (2 МБ) с уникальным паттерном для проверки целостности
    constexpr size_t total_size = 2 * 1024 * 1024;
    std::vector<uint8_t> big_payload(total_size);
    for (size_t i = 0; i < total_size; ++i) {
        big_payload[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);
    }

    // Отправляем всё одним вызовом: часть уйдёт в ядро, остаток ляжет в out_queue_
    assert(dispatcher.send(pair.server_fd, big_payload) == true);
    assert(dispatcher.has_pending(pair.server_fd) == true);

    // Вычитываем данные клиентом и параллельно крутим событийный цикл
    std::vector<uint8_t> received;
    received.reserve(total_size);
    std::vector<uint8_t> chunk(65536);

    while (received.size() < total_size) {
        // Клиент вычитывает доступную порцию из сокета
        ssize_t r = recv(pair.client_fd, chunk.data(), chunk.size(), MSG_DONTWAIT);
        if (r > 0) {
            received.insert(received.end(), chunk.begin(), chunk.begin() + r);
        }

        // Ждём события готовности к записи на сервере
        net_event_t events[4];
        int n = net_loop_wait(loop, events, 4, 20);
        for (int i = 0; i < n; ++i) {
            if (events[i].fd == pair.server_fd && events[i].event_type == NET_EVENT_WRITE) {
                assert(dispatcher.on_writable(pair.server_fd) == true);
            }
        }
    }

    // После полной отправки всех 2 МБ очередь должна опустеть
    assert(dispatcher.has_pending(pair.server_fd) == false);
    assert(received == big_payload);

    // Убеждаемся, что EPOLLOUT выключен и лишних событий больше нет
    net_event_t ev{};
    assert(net_loop_wait(loop, &ev, 1, 10) == 0);

    pair.close_all();
    net_loop_destroy(loop);
    std::cout << "[PASS] test_partial_write_and_flush\n";
}

// Тест сохранения порядка FIFO при добавлении данных в уже непустую очередь
void test_fifo_ordering_with_pending_queue() {
    net_loop_t* loop = net_loop_create();
    TcpPair pair = TcpPair::create(loop);

    int sndbuf = 16384;
    setsockopt(pair.server_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    net_dispatcher dispatcher(loop);

    // Забиваем буфер первой большой посылкой (1 МБ единиц)
    std::vector<uint8_t> part1(1024 * 1024, 0x11);
    assert(dispatcher.send(pair.server_fd, part1) == true);
    assert(dispatcher.has_pending(pair.server_fd) == true);

    // Пока очередь не пуста, отправляем вторую и третью посылки
    // Они не должны проскочить в сокет раньше хвоста part1!
    std::vector<uint8_t> part2(4096, 0x22);
    std::vector<uint8_t> part3 = {0xAA, 0xBB, 0xCC, 0xDD};

    assert(dispatcher.send(pair.server_fd, part2) == true);
    assert(dispatcher.send(pair.server_fd, part3) == true);
    assert(dispatcher.has_pending(pair.server_fd) == true);

    // Вычитываем всё на клиенте и прокачиваем события
    const size_t expected_total = part1.size() + part2.size() + part3.size();
    std::vector<uint8_t> received;
    received.reserve(expected_total);
    std::vector<uint8_t> buf(65536);

    while (received.size() < expected_total) {
        ssize_t r = recv(pair.client_fd, buf.data(), buf.size(), MSG_DONTWAIT);
        if (r > 0) {
            received.insert(received.end(), buf.begin(), buf.begin() + r);
        }

        net_event_t events[4];
        int n = net_loop_wait(loop, events, 4, 20);
        for (int i = 0; i < n; ++i) {
            if (events[i].fd == pair.server_fd && events[i].event_type == NET_EVENT_WRITE) {
                assert(dispatcher.on_writable(pair.server_fd) == true);
            }
        }
    }

    assert(dispatcher.has_pending(pair.server_fd) == false);
    assert(received.size() == expected_total);

    // Проверяем строгую последовательность: сначала все 0x11, затем 0x22, в конце 0xAA..0xDD
    for (size_t i = 0; i < part1.size(); ++i) {
        assert(received[i] == 0x11);
    }
    for (size_t i = 0; i < part2.size(); ++i) {
        assert(received[part1.size() + i] == 0x22);
    }
    size_t tail_offset = part1.size() + part2.size();
    assert(received[tail_offset + 0] == 0xAA);
    assert(received[tail_offset + 1] == 0xBB);
    assert(received[tail_offset + 2] == 0xCC);
    assert(received[tail_offset + 3] == 0xDD);

    pair.close_all();
    net_loop_destroy(loop);
    std::cout << "[PASS] test_fifo_ordering_with_pending_queue\n";
}

// Тест очистки очереди и повторного использования fd
void test_clear_queue() {
    net_loop_t* loop = net_loop_create();
    TcpPair pair = TcpPair::create(loop);

    int sndbuf = 16384;
    setsockopt(pair.server_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    net_dispatcher dispatcher(loop);

    // Очистка для fd, которого нет в мапе, не должна падать
    dispatcher.clear_queue(9999);
    assert(dispatcher.has_pending(9999) == false);

    // Создаём отложенную очередь
    std::vector<uint8_t> big_data(1024 * 1024, 0x55);
    assert(dispatcher.send(pair.server_fd, big_data) == true);
    assert(dispatcher.has_pending(pair.server_fd) == true);

    // Сбрасываем очередь
    dispatcher.clear_queue(pair.server_fd);
    assert(dispatcher.has_pending(pair.server_fd) == false);

    // Повторный вызов clear_queue безопасен
    dispatcher.clear_queue(pair.server_fd);

    pair.close_all();
    net_loop_destroy(loop);
    std::cout << "[PASS] test_clear_queue\n";
}

// Тест граничных случаев и фатальных ошибок сокета
void test_error_handling_and_edge_cases() {
    net_loop_t* loop = net_loop_create();
    TcpPair pair = TcpPair::create(loop);
    net_dispatcher dispatcher(loop);

    // Отправка пустого span считается невалидным параметром и возвращает false
    std::span<const uint8_t> empty_span{};
    assert(dispatcher.send(pair.server_fd, empty_span) == false);
    assert(dispatcher.has_pending(pair.server_fd) == false);

    // Вызов on_writable, когда в очереди ничего нет, возвращает false и не ломает состояние
    assert(dispatcher.on_writable(pair.server_fd) == false);

    // Отправка в невалидный дескриптор должна вернуть false
    const std::vector<uint8_t> data = {1, 2, 3};
    assert(dispatcher.send(-1, data) == false);
    assert(dispatcher.has_pending(-1) == false);

    // Принудительно закрываем клиентский сокет с RST, чтобы вызвать ошибку отправки на сервере
    linger lg{1, 0};
    setsockopt(pair.client_fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    close(pair.client_fd);
    pair.client_fd = -1;

    // Даём ядру обработать RST (первый send может ещё пройти в буфер, второй вернёт ошибку)
    bool failed = false;
    for (int i = 0; i < 10; ++i) {
        if (!dispatcher.send(pair.server_fd, data)) {
            failed = true;
            break;
        }
        usleep(1000);
    }
    assert(failed == true);
    pair.close_all();
    net_loop_destroy(loop);
    std::cout << "[PASS] test_error_handling_and_edge_cases\n";
}

// Тест одновременной буферизованной отправки в несколько сокетов (мультиплексирование очередей)
void test_multiple_sockets_multiplexing() {
    net_loop_t* loop = net_loop_create();
    assert(loop != nullptr);

    constexpr int NUM_CLIENTS = 3;
    std::vector<TcpPair> pairs(NUM_CLIENTS);
    std::vector<std::vector<uint8_t>> payloads(NUM_CLIENTS);
    std::vector<std::vector<uint8_t>> received(NUM_CLIENTS);

    // Разные размеры и уникальные паттерны для каждого соединения
    const size_t sizes[NUM_CLIENTS] = {512 * 1024, 1024 * 1024, 1536 * 1024};

    for (int i = 0; i < NUM_CLIENTS; ++i) {
        pairs[i] = TcpPair::create(loop);

        // Уменьшаем буферы отправки, чтобы все 3 сокета ушли в частичную запись
        int sndbuf = 16384;
        setsockopt(pairs[i].server_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        payloads[i].resize(sizes[i]);
        for (size_t j = 0; j < sizes[i]; ++j) {
            // Уникальный паттерн, привязанный к индексу клиента i
            payloads[i][j] = static_cast<uint8_t>((j + i * 73) & 0xFF);
        }
        received[i].reserve(sizes[i]);
    }

    net_dispatcher dispatcher(loop);

    // Запускаем отправку во все 3 сокета подряд
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        assert(dispatcher.send(pairs[i].server_fd, payloads[i]) == true);
        assert(dispatcher.has_pending(pairs[i].server_fd) == true);
    }

    // Прокачиваем цикл до тех пор, пока все 3 клиента не получат свои данные целиком
    std::vector<uint8_t> tmp_buf(65536);
    while (true) {
        bool all_done = true;
        for (int i = 0; i < NUM_CLIENTS; ++i) {
            if (received[i].size() < payloads[i].size()) {
                all_done = false;
                // Имитируем разную скорость чтения: клиент 2 читает меньшими порциями
                size_t max_read = (i == 2) ? 8192 : tmp_buf.size();
                ssize_t r = recv(pairs[i].client_fd, tmp_buf.data(), max_read, MSG_DONTWAIT);
                if (r > 0) {
                    received[i].insert(received[i].end(), tmp_buf.begin(), tmp_buf.begin() + r);
                }
            }
        }
        if (all_done) {
            break;
        }

        net_event_t events[8];
        int n = net_loop_wait(loop, events, 8, 20);
        for (int k = 0; k < n; ++k) {
            if (events[k].event_type == NET_EVENT_WRITE) {
                assert(dispatcher.on_writable(events[k].fd) == true);
            }
        }
    }

    // Проверяем, что у всех сокетов очереди полностью очистились и данные не перемешались
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        assert(dispatcher.has_pending(pairs[i].server_fd) == false);
        assert(received[i] == payloads[i]);
        pairs[i].close_all();
    }

    // Убеждаемся, что после завершения всех отправок лишних событий EPOLLOUT нет
    net_event_t ev{};
    assert(net_loop_wait(loop, &ev, 1, 10) == 0);

    net_loop_destroy(loop);
    std::cout << "[PASS] test_multiple_sockets_multiplexing\n";
}

int main() {
    test_immediate_send();
    test_partial_write_and_flush();
    test_fifo_ordering_with_pending_queue();
    test_clear_queue();
    test_error_handling_and_edge_cases();
    test_multiple_sockets_multiplexing();
    std::cout << "\nAll net_dispatcher tests passed successfully!\n";
    return 0;
}