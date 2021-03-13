#ifndef _ROUTING_HPP
#define _ROUTING_HPP

#include "router.hpp"

#include "handlers.hpp"

namespace bserv {

bserv::router routes{
    bserv::make_path("/", &hello,
        bserv::placeholders::response,
        bserv::placeholders::session),
    bserv::make_path("/register", &user_register,
        bserv::placeholders::request,
        bserv::placeholders::json_params,
        bserv::placeholders::transaction),
    bserv::make_path("/login", &user_login,
        bserv::placeholders::request,
        bserv::placeholders::json_params,
        bserv::placeholders::transaction,
        bserv::placeholders::session),
    bserv::make_path("/logout", &user_logout,
        bserv::placeholders::session),
    bserv::make_path("/find/<str>", &find_user,
        bserv::placeholders::transaction,
        bserv::placeholders::_1),
    bserv::make_path("/send", &send_request)
};

}  // bserv

#endif  // _ROUTING_HPP