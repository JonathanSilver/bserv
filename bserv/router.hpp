#ifndef _ROUTER_HPP
#define _ROUTER_HPP

#include <boost/beast.hpp>
#include <boost/json/src.hpp>

#include <string>
#include <regex>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>
#include <optional>

#include <pqxx/pqxx>

#include "client.hpp"
#include "database.hpp"
#include "session.hpp"
#include "utils.hpp"
#include "config.hpp"

namespace bserv {

namespace beast = boost::beast;
namespace http = beast::http;

struct server_resources {
    std::shared_ptr<session_manager_base> session_mgr;
    std::shared_ptr<db_connection_manager> db_conn_mgr;
    std::shared_ptr<http_client> http_client_ptr;
};

namespace placeholders {

template <int N>
struct placeholder {};

#define make_place_holder(x) constexpr placeholder<x> _##x

make_place_holder(1);
make_place_holder(2);
make_place_holder(3);
make_place_holder(4);
make_place_holder(5);
make_place_holder(6);
make_place_holder(7);
make_place_holder(8);
make_place_holder(9);

#undef make_place_holder

// std::shared_ptr<bserv::session_type>
constexpr placeholder<-1> session;
// bserv::request_type&
constexpr placeholder<-2> request;
// bserv::response_type&
constexpr placeholder<-3> response;
// boost::json::object&&
constexpr placeholder<-4> json_params;
// std::shared_ptr<bserv::db_connection>
constexpr placeholder<-5> db_connection_ptr;
// std::shared_ptr<bserv::http_client>
constexpr placeholder<-6> http_client_ptr;

}  // placeholders

class bad_request_exception : public std::exception {
public:
    bad_request_exception() = default;
    const char* what() const noexcept { return "bad request"; }
};

namespace router_internal {

template <typename ...Types>
struct parameter_pack;

template <>
struct parameter_pack<> {};

template <typename Head, typename ...Tail>
struct parameter_pack<Head, Tail...>
    : parameter_pack<Tail...> {
    Head head_;
    template <typename Head2, typename ...Tail2>
    parameter_pack(Head2&& head, Tail2&& ...tail)
        : parameter_pack<Tail...>{static_cast<Tail2&&>(tail)...},
        head_{static_cast<Head2&&>(head)} {}
};

template <int Idx, typename ...Types>
struct get_parameter_pack;

template <int Idx, typename Head, typename ...Tail>
struct get_parameter_pack<Idx, Head, Tail...>
    : get_parameter_pack<Idx - 1, Tail...> {};

template <typename Head, typename ...Tail>
struct get_parameter_pack<0, Head, Tail...> {
    using type = parameter_pack<Head, Tail...>;
};

template <int Idx, typename ...Types>
decltype(auto) get_parameter_value(parameter_pack<Types...>& params) {
    return (static_cast<
        typename get_parameter_pack<Idx, Types...>::type&
    >(params).head_);
}

template <int Idx, typename ...Types>
struct get_parameter;

template <int Idx, typename Head, typename ...Tail>
struct get_parameter<Idx, Head, Tail...>
    : get_parameter<Idx - 1, Tail...> {};

template <typename Head, typename ...Tail>
struct get_parameter<0, Head, Tail...> {
    using type = Head;
};

template <typename Type>
Type&& get_parameter_data(
    server_resources&,
    const std::vector<std::string>&,
    request_type&, response_type&, Type&& val) {
    return static_cast<Type&&>(val);
}

template <int N, std::enable_if_t<(N >= 0), int> = 0>
const std::string& get_parameter_data(
    server_resources&,
    const std::vector<std::string>& url_params,
    request_type&, response_type&,
    placeholders::placeholder<N>) {
    return url_params[N];
}

std::shared_ptr<session_type> get_parameter_data(
    server_resources& resources,
    const std::vector<std::string>&,
    request_type& request, response_type& response,
    placeholders::placeholder<-1>) {
    std::string cookie_str{request[http::field::cookie]};
    auto&& [cookie_dict, cookie_list] 
        = utils::parse_params(cookie_str, 0, ';');
    boost::ignore_unused(cookie_list);
    std::string session_id;
    if (cookie_dict.count(SESSION_NAME) != 0) {
        session_id = cookie_dict[SESSION_NAME];
    }
    std::shared_ptr<session_type> session_ptr;
    if (resources.session_mgr->get_or_create(session_id, session_ptr)) {
        response.set(http::field::set_cookie, SESSION_NAME + "=" + session_id);
    }
    return session_ptr;
}

request_type& get_parameter_data(
    server_resources&,
    const std::vector<std::string>&,
    request_type& request, response_type&,
    placeholders::placeholder<-2>) {
    return request;
}

response_type& get_parameter_data(
    server_resources&,
    const std::vector<std::string>&,
    request_type&, response_type& response,
    placeholders::placeholder<-3>) {
    return response;
}

boost::json::object get_parameter_data(
    server_resources&,
    const std::vector<std::string>&,
    request_type& request, response_type&,
    placeholders::placeholder<-4>) {
    std::string target{request.target()};
    auto&& [url, dict_params, list_params] = utils::parse_url(target);
    boost::ignore_unused(url);
    boost::json::object body;
    if (!request.body().empty()) {
        try {
            body = boost::json::parse(request.body()).as_object();
        } catch (const std::exception& e) {
            throw bad_request_exception{};
        }
    }
    for (auto& [k, v] : dict_params) {
        if (!body.contains(k)) {
            body[k] = v;
        }
    }
    for (auto& [k, vs] : list_params) {
        if (!body.contains(k)) {
            boost::json::array a;
            for (auto& v : vs) {
                a.push_back(boost::json::string{v});
            }
            body[k] = a;
        }
    }
    return body;
}

std::shared_ptr<db_connection> get_parameter_data(
    server_resources& resources,
    const std::vector<std::string>&,
    request_type&, response_type&,
    placeholders::placeholder<-5>) {
    return resources.db_conn_mgr->get_or_block();
}

std::shared_ptr<http_client> get_parameter_data(
    server_resources& resources,
    const std::vector<std::string>&,
    request_type&, response_type&,
    placeholders::placeholder<-6>) {
    return resources.http_client_ptr;
}

template <int Idx, typename Func, typename Params, typename ...Args>
struct path_handler;

template <int Idx, typename Ret, typename ...Args, typename ...Params>
struct path_handler<Idx, Ret (*)(Args ...), parameter_pack<Params...>> {
    Ret invoke(server_resources& resources,
        Ret (*pf)(Args ...), parameter_pack<Params...>& params,
        const std::vector<std::string>& url_params,
        request_type& request, response_type& response) {
        if constexpr (Idx == 0) return (*pf)();
        else return static_cast<path_handler<
            Idx - 1, Ret (*)(Args ...), parameter_pack<Params...>,
            typename get_parameter<Idx - 1, Params...>::type>*
            >(this)->invoke2(resources, pf, params, url_params, request, response,
                get_parameter_data(resources, url_params, request, response,
                    get_parameter_value<Idx - 1>(params)));
    }
};

template <int Idx, typename Ret, typename ...Args,
    typename ...Params, typename Head, typename ...Tail>
struct path_handler<Idx, Ret (*)(Args ...),
    parameter_pack<Params...>, Head, Tail...>
    : path_handler<Idx + 1, Ret (*)(Args ...),
        parameter_pack<Params...>, Tail...> {
    template <
        typename Head2, typename ...Tail2,
        std::enable_if_t<sizeof...(Tail2) == sizeof...(Tail), int> = 0>
    Ret invoke2(server_resources& resources,
        Ret (*pf)(Args ...), parameter_pack<Params...>& params, 
        const std::vector<std::string>& url_params,
        request_type& request, response_type& response,
        Head2&& head2, Tail2&& ...tail2) {
        if constexpr (Idx == 0)
            return (*pf)(static_cast<Head2&&>(head2),
                static_cast<Tail2&&>(tail2)...);
        else return static_cast<path_handler<
            Idx - 1, Ret (*)(Args ...), parameter_pack<Params...>,
            typename get_parameter<Idx - 1, Params...>::type, Head, Tail...>*
            >(this)->invoke2(resources, pf, params, url_params, request, response,
                get_parameter_data(resources, url_params, request, response,
                    get_parameter_value<Idx - 1>(params)),
                static_cast<Head2&&>(head2), static_cast<Tail2&&>(tail2)...);
    }
};

const std::vector<std::pair<std::regex, std::string>> url_regex_mapping{
    {std::regex{"<int>"}, "([0-9]+)"},
    {std::regex{"<str>"}, R"(([A-Za-z0-9_\.\-]+))"},
    {std::regex{"<path>"}, R"(([A-Za-z0-9_/\.\-]+))"}
};

std::string get_re_url(const std::string& url) {
    std::string re_url = url;
    for (auto& [r, s] : url_regex_mapping)
        re_url = std::regex_replace(re_url, r, s);
    return re_url;
}

struct path_holder : std::enable_shared_from_this<path_holder> {
    path_holder() = default;
    virtual ~path_holder() = default;
    virtual bool match(
        const std::string&,
        std::vector<std::string>&) const = 0;
    virtual std::optional<boost::json::value> invoke(
        server_resources&,
        const std::vector<std::string>&,
        request_type&, response_type&) = 0;
};

template <typename Func, typename Params>
class path;

template <typename Ret, typename ...Args, typename ...Params>
class path<Ret (*)(Args ...), parameter_pack<Params...>>
    : public path_holder {
private:
    std::regex re_;
    Ret (*pf_)(Args ...);
    parameter_pack<Params...> params_;
    path_handler<0, Ret (*)(Args ...), parameter_pack<Params...>, Params...> handler_;
public:
    path(const std::string& url, Ret (*pf)(Args ...), Params&& ...params)
        : re_{get_re_url(url)}, pf_{pf},
        params_{static_cast<Params&&>(params)...} {}
    bool match(const std::string& url, std::vector<std::string>& result) const {
        std::smatch r;
        bool matched = std::regex_match(url, r, re_);
        if (matched) {
            result.clear();
            for (auto & sub : r)
                result.push_back(sub.str());
        }
        return matched;
    }
    std::optional<boost::json::value> invoke(
        server_resources& resources,
        const std::vector<std::string>& url_params,
        request_type& request, response_type& response) {
        return handler_.invoke(
            resources,
            pf_, params_, url_params,
            request, response);
    }
};

} // router_internal

template <typename Ret, typename ...Args, typename ...Params>
std::shared_ptr<router_internal::path<Ret (*)(Args ...),
    router_internal::parameter_pack<Params...>>> make_path(
    const std::string& url, Ret (*pf)(Args ...), Params&& ...params) {
    return std::make_shared<
        router_internal::path<Ret (*)(Args ...),
        router_internal::parameter_pack<Params...>>
    >(url, pf, static_cast<Params&&>(params)...);
}

template <typename Ret, typename ...Args, typename ...Params>
std::shared_ptr<router_internal::path<Ret (*)(Args ...),
    router_internal::parameter_pack<Params...>>> make_path(
    const char* url, Ret (*pf)(Args ...), Params&& ...params) {
    return std::make_shared<
        router_internal::path<Ret (*)(Args ...),
        router_internal::parameter_pack<Params...>>
    >(url, pf, static_cast<Params&&>(params)...);
}

class url_not_found_exception : public std::exception {
public:
    url_not_found_exception() = default;
    const char* what() const noexcept { return "url not found"; }
};

class router {
private:
    using path_holder_type = std::shared_ptr<router_internal::path_holder>;
    std::vector<path_holder_type> paths_;
    std::shared_ptr<server_resources> resources_;
public:
    router(const std::initializer_list<path_holder_type>& paths)
        : paths_{paths} {}
    void set_resources(std::shared_ptr<server_resources> resources) {
        resources_ = resources;
    }
    std::optional<boost::json::value> operator()(
        const std::string& url, request_type& request, response_type& response) {
        std::vector<std::string> url_params;
        for (auto& ptr : paths_) {
            if (ptr->match(url, url_params))
                return ptr->invoke(*resources_, url_params, request, response);
        }
        throw url_not_found_exception{};
    }
};

}  // bserv

#endif  // _ROUTER_HPP