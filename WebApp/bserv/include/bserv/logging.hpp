#ifndef _LOGGING_HPP
#define _LOGGING_HPP

#if defined(__GNUC__)
#define BOOST_LOG_DYN_LINK
#endif

#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>

#include <iostream>
#include <cstddef>
#include <string>

#include "config.hpp"

#define lgtrace BOOST_LOG_TRIVIAL(trace)
#define lgdebug BOOST_LOG_TRIVIAL(debug)
#define lginfo BOOST_LOG_TRIVIAL(info)
#define lgwarning BOOST_LOG_TRIVIAL(warning)
#define lgerror BOOST_LOG_TRIVIAL(error)
#define lgfatal BOOST_LOG_TRIVIAL(fatal)

namespace bserv {

	namespace logging = boost::log;
	namespace keywords = boost::log::keywords;
	namespace src = boost::log::sources;

	// this function should be called before logging is used
	inline void init_logging(const server_config& config) {
		if (config.get_log_path() != "") {
			std::string filename = config.get_log_path();
			if (filename[filename.size() - 1] != '/') {
				filename += '/';
			}
			filename += config.get_name();
			logging::add_file_log(
				keywords::file_name = filename + "_%Y%m%d_%H-%M-%S.%N.log",
				keywords::rotation_size = config.get_log_rotation_size(),
				keywords::format = "[%Severity%][%TimeStamp%][%ThreadID%]: %Message%"
			);
#if defined(_MSC_VER) && defined(_DEBUG)
			// write to console as well
			logging::add_console_log(std::cout);
#endif
		}
		logging::core::get()->set_filter(
#if defined(_MSC_VER) && defined(_DEBUG)
			logging::trivial::severity >= logging::trivial::trace
#else
			logging::trivial::severity >= logging::trivial::info
#endif
		);
		logging::add_common_attributes();
	}

	inline void fail(const boost::system::error_code& ec, const char* what) {
		lgerror << what << ": " << ec.message() << std::endl;
	}

}  // bserv

#endif  // _LOGGING_HPP
