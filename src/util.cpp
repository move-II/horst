#include "util.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <poll.h>
#include <sstream>
#include <uv.h>


namespace horst {
namespace util {


std::string demangle(const char *symbol) {
	int status;
	char *buf = abi::__cxa_demangle(symbol, nullptr, nullptr, &status);

	if (status != 0) {
		return symbol;
	} else {
		std::string result{buf};
		free(buf);
		return result;
	}
}


static std::string addr_to_string(const void *addr) {
	std::ostringstream out;
	out << "[" << addr << "]";
	return out.str();
}


std::string symbol_name(const void *addr,
                        bool require_exact_addr, bool no_pure_addrs) {
	Dl_info addr_info;

	if (dladdr(addr, &addr_info) == 0) {
		// dladdr has... failed.
		return no_pure_addrs ? "" : addr_to_string(addr);
	} else {
		size_t symbol_offset = reinterpret_cast<size_t>(addr) -
		                       reinterpret_cast<size_t>(addr_info.dli_saddr);

		if (addr_info.dli_sname == nullptr or
		    (symbol_offset != 0 and require_exact_addr)) {

			return no_pure_addrs ? "" : addr_to_string(addr);
		}

		if (symbol_offset == 0) {
			// this is our symbol name.
			return demangle(addr_info.dli_sname);
		} else {
			std::ostringstream out;
			out << demangle(addr_info.dli_sname)
			    << "+0x" << std::hex
			    << symbol_offset << std::dec;
			return out.str();
		}
	}
}


int poll_to_libuv_events(int pollflags) {
	int ret = 0;
	if (pollflags & (POLLIN | POLLPRI)) {
		ret |= UV_READABLE;
	}
	if (pollflags & POLLOUT) {
		ret |= UV_WRITABLE;
	}

	// we also have the non-corresponding UV_DISCONNECT

	return ret;
}

int exec(const std::string& cmd) {
	FILE* fd = popen(cmd.c_str(), "r");
	int exit = 100;
	if (fd != NULL) {
		exit = pclose(fd);
	}
	return WEXITSTATUS(exit);
}

}} // horst::util
