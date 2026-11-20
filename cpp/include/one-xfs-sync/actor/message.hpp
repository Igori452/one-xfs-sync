#include <variant>


struct M {
    int payload;
};


using message = std::variant<M>; 
