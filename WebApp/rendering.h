#pragma once

#include <string>
#include <optional>

#include <boost/json.hpp>
#include "bserv/common.hpp"

void init_rendering(const std::string& template_root);

void init_static_root(const std::string& static_root);

std::nullopt_t render(
	bserv::response_type& response,
	const std::string& template_path,
	const boost::json::object& context = {}
);

std::nullopt_t serve(
	bserv::response_type& response,
	const std::string& file
);
