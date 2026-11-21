#include "one-xfs-sync/core/connection_core.hpp"
#include "one-xfs-sync/actor/actor.hpp"

class connection_actor : public actor
{
    private:
        /**
         * TODO:
         * Поле с сылкой на оркестратора.
         */
        connection_core connection_core_;

         /**
         * Главная функция которая слушает новых клиентов и 
         * инициирует создание новых пулов, вызывая send 
         * в свою же очередь о том что пришли новые клиенты или
         * запросы от этих клиентов.
         */
        void accept_loop();
    protected:
        void on_receive(const message& msg) override;

    public:
};