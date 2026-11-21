#include <variant>


struct message_new_connection {
    std::size_t session_id;
    std::size_t connection_pool_size;
};

struct message_client_request {
    std::string request_path;
    /**
     * TODO:
     * Вставить поле send_to по которому определенный хендлер будет
     * понимать куда необходимо переслать данное сообщение. Актор
     * знает куда он хочет его отправить но core ничего не знает про 
     * логику акторов.
     */
};


using message = std::variant<message_new_connection,
                            message_client_request>; 
