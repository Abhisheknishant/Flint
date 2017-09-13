/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gui/httpd.h"

#include <cstdio>
#include <cstring>
#include <thread>

#include <microhttpd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <wx/wx.h>
#include <wx/filename.h>
#pragma GCC diagnostic pop

#include "gui/main-frame.h"
#include "run.h"
#include "utf8path.h"

namespace flint {
namespace gui {

namespace {

int Respond(MHD_Connection *connection, int status)
{
	MHD_Response *response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
	int r = MHD_queue_response(connection, status, response);
	MHD_destroy_response(response);
	return r;
}

void Run(std::unique_ptr<cli::RunOption> &&option)
{
	auto o = std::move(option);

	wxFileName fileName;
	fileName.AssignHomeDir();
	fileName.AppendDir(".flint");
	fileName.AppendDir("2");
	auto now = wxDateTime::Now();
	fileName.AppendDir(now.Format("%F %T"));
	fileName.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL); // make sure that it exists
	auto dirPath = fileName.GetFullPath();
	run::Run(*o, dirPath.c_str().AsChar());
}

int AccessHandler(void *cls,
				  MHD_Connection *connection,
				  const char *url,
				  const char *method,
				  const char */*version*/,
				  const char *upload_data,
				  size_t *upload_data_size,
				  void **con_cls)
{
	const size_t kMaxDataSize = 8192;
	thread_local int status = MHD_HTTP_BAD_REQUEST;

	if (std::strcmp(method, "POST") != 0)
		return MHD_NO;
	if (*con_cls == nullptr) {
		*con_cls = &status;
		return MHD_YES;
	}
	if (*upload_data_size == 0) {
		*con_cls = nullptr;
		return Respond(connection, status);
	}
	if (*upload_data_size > kMaxDataSize) {
		status = MHD_HTTP_PAYLOAD_TOO_LARGE;
		*upload_data_size = 0;
		return MHD_YES;
	}
	if (std::strcmp(url, "/open-model") == 0) {
		std::unique_ptr<char[]> buf(new char[*upload_data_size+1]);
		std::strcpy(buf.get(), upload_data);
		auto path = GetPathFromUtf8(buf.get());
		auto *frame = static_cast<MainFrame *>(cls);
		auto lmbd = [frame, path]{frame->OpenFile(path.string());};
		frame->GetEventHandler()->CallAfter(lmbd);
		status = MHD_HTTP_OK;
		*upload_data_size = 0;
		return MHD_YES;
	}
	if (std::strcmp(url, "/run") == 0) {
		std::unique_ptr<cli::RunOption> option(new cli::RunOption);
		if (option->ParseFromArray(upload_data, *upload_data_size)) {
			std::thread th(Run, std::move(option));
			th.detach();
			status = MHD_HTTP_OK;
		} else {
			status = MHD_HTTP_BAD_REQUEST;
		}
		*upload_data_size = 0;
		return MHD_YES;
	}
	status = MHD_HTTP_NOT_FOUND;
	*upload_data_size = 0;
	return MHD_YES;
}

}

Httpd::Httpd()
	: daemon_(nullptr)
{}

Httpd::~Httpd()
{
	MHD_stop_daemon(static_cast<MHD_Daemon *>(daemon_));
}

bool Httpd::Start(MainFrame *frame)
{
	const int kPort = 20465;
	daemon_ = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
							   kPort,
							   nullptr,
							   nullptr,
							   &AccessHandler,
							   frame,
							   MHD_OPTION_END);
	return daemon_ != nullptr;
}

}
}