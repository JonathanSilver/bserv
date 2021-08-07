#ifndef _LOGGING_HPP
#define _LOGGING_HPP

#define BOOST_LOG_DYN_LINK

#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>

#include <cstddef>
#include <string>

#include "config.hpp"

namespace bserv {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace src = boost::log::sources;

// this function should be called before logging is used
inline void init_logging(const server_config& config) {
    logging::add_file_log(
        keywords::file_name = config.get_log_path() + "_%Y%m%d_%H-%M-%S.%N.log",
        keywords::rotation_size = config.get_log_rotation_size(),
        keywords::format = "[%Severity%][%TimeStamp%][%ThreadID%]: %Message%"
    );
    logging::core::get()->set_filter(
        logging::trivial::severity >= logging::trivial::trace
    );
    logging::add_common_attributes();
}

#define lgtrace BOOST_LOG_TRIVIAL(trace)
#define lgdebug BOOST_LOG_TRIVIAL(debug)
#define lginfo BOOST_LOG_TRIVIAL(info)
#define lgwarning BOOST_LOG_TRIVIAL(warning)
#define lgerror BOOST_LOG_TRIVIAL(error)
#define lgfatal BOOST_LOG_TRIVIAL(fatal)

inline void fail(const boost::system::error_code& ec, const char* what) {
    lgerror << what << ": " << ec.message() << std::endl;
}

}  // bserv

#endif  // _LOGGING_HPP