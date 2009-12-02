#include "stdafx.h"
// isatty
#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif
#include "debugger_cli.h"

namespace po = boost::program_options;

boost::cli::commands_description debugger_cli::m_desc = boost::cli::commands_description();

debugger_cli::debugger_cli()
 : m_tracer()
{
#ifdef _MSC_VER
	m_interactive = _isatty(_fileno(stdin)) != 0;
#else
	m_interactive = isatty(fileno(stdin)) != 0;
#endif

	m_desc.add_options()
		(
			"show",
			po::value<std::string>()->implicit_value("info")->notifier(boost::bind(&debugger_cli::show_handler, this, _1)),
			"show informational messages"
		)

		(
			"help,?",
			po::value<std::string>()->implicit_value("")->notifier(boost::bind(&debugger_cli::help_handler, this, _1)),
			"show this message"
		)

		(
			"load,l",
			po::value<std::string>()->notifier(boost::bind(&debugger_cli::load_handler, this, _1)),
			"load script file(s)"
		)

		(
			"start,s",
			po::value<std::string>()->notifier(boost::bind(&debugger_cli::start_handler, this, _1)),
			"start process(es)"
		)

		(
			"trace",
			po::value<std::string>()->notifier(boost::bind(&debugger_cli::trace_handler, this, _1)),
			"trace process(es)"
		)

		(
			"quit",
			po::value<int>()->implicit_value(0)->notifier(boost::bind(&debugger_cli::exit_handler, this, _1)),
			"close opendbg"
		)

		(
			"next",
			po::value<std::string>()->implicit_value("event")->notifier(boost::bind(&debugger_cli::next_handler, this, _1)),
			"next event"
		)
		;

	m_tracer.add_trace_slot(boost::bind(&debugger_cli::debug_slot, this, _1));
	m_tracer.add_breakpoint_slot(boost::bind(&debugger_cli::breakpoint_slot, this, _1));
	m_tracer.add_terminated_slot(boost::bind(&debugger_cli::terminated_slot, this, _1));
	m_tracer.add_start_thread_slot(boost::bind(&debugger_cli::start_thread_slot, this, _1));
	m_tracer.add_exit_thread_slot(boost::bind(&debugger_cli::exit_thread_slot, this, _1));
	m_tracer.add_exception_slot(boost::bind(&debugger_cli::exception_slot, this, _1));
	m_tracer.add_dll_load_slot(boost::bind(&debugger_cli::dll_load_slot, this, _1));

	m_cli = new boost::cli::command_line_interpreter(m_desc, m_interactive ? "> " : "");
}

debugger_cli::~debugger_cli(void)
{
	delete m_cli;
}

void debugger_cli::load_handler(const std::string& filename)
{
	std::cout << "script loading is not supported\n";
}

void debugger_cli::start_handler(const std::string& filename)
{
}

void debugger_cli::trace_handler(const std::string& param)
{
	m_tracer.set_image_name(param);
	boost::thread dbg_thread(boost::ref(m_tracer));
	//dbg_thread.join();
}

void debugger_cli::help_handler(const std::string& param)
{
	// TODO: normal description
	//cout << desc;
}

void debugger_cli::show_handler(const std::string& param)
{
	if (param == "w")
	{
		std::cout << "This program is distributed in the hope that it will be useful,\n";
		std::cout << "but WITHOUT ANY WARRANTY; without even the implied warranty of\n";
		std::cout << "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n";
		std::cout << "GNU General Public License for more details.\n";
	}
	else if (param == "c")
	{
		std::cout << "This program is free software: you can redistribute it and/or modify\n";
		std::cout << "it under the terms of the GNU General Public License as published by\n";
		std::cout << "the Free Software Foundation, either version 3 of the License, or\n";
		std::cout << "(at your option) any later version.\n";
	}
	else if (param == "info")
	{
		std::cout << "please supply one of arguments:\n";
		std::cout << "\tinfo\tshow this message\n";
		std::cout << "\tw\tshow gpl warranty message\n";
		std::cout << "\tc\tshow gpl free software message\n";
	}
}

void debugger_cli::exit_handler(int code)
{
	exit(code);
}

void debugger_cli::next_handler(const std::string& param)
{
	m_condition.notify_one();
}

void debugger_cli::interpret(std::istream& input_stream)
{
	m_cli->interpret(input_stream);
}

void debugger_cli::start()
{
	m_cli->interpret(std::cin);
}

void debugger_cli::debug_slot(dbg_msg msg)
{
	lock_t lock(m_mutex);
	m_condition.wait(lock);
	//std::cout << msg.event_code << "\n";
}

void debugger_cli::breakpoint_slot( dbg_msg msg )
{
	lock_t lock(m_mutex);
	m_condition.wait(lock);
}

void debugger_cli::terminated_slot( dbg_msg msg )
{
	printf("DBG_TERMINATED %x by %x\n",
					msg.terminated.proc_id,
					msg.process_id
					);
}

void debugger_cli::start_thread_slot( dbg_msg msg )
{
	printf("DBG_START_THREAD %x by %x, teb: %x\n",
					msg.thread_start.thread_id,
					msg.process_id,
					msg.thread_start.teb_addr
					);
}

void debugger_cli::exit_thread_slot( dbg_msg msg )
{
	printf("DBG_EXIT_THREAD %x in %x by %x\n",
					msg.thread_exit.thread_id,
					msg.thread_exit.proc_id,
					msg.process_id
					);
}

void debugger_cli::exception_slot( dbg_msg msg )
{
	printf("DBG_EXCEPTION %0.8x in %x:%x\n",
					msg.exception.except_record.ExceptionCode,
					msg.thread_id,
					msg.process_id
					);

	lock_t lock(m_mutex);
	m_condition.wait(lock);
}

void debugger_cli::dll_load_slot( dbg_msg msg )
{
	printf("DBG_LOAD_DLL %ws adr 0x%p sz 0x%x in %x:%x\n",
		msg.dll_load.dll_name,
		msg.dll_load.dll_image_base,
		msg.dll_load.dll_image_size,
		msg.thread_id,
		msg.process_id
		);
}

