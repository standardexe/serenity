/*
 * Copyright (c) 2020-2021, Linus Groh <linusg@serenityos.org>
 * Copyright (c) 2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/LexicalPath.h>
#include <AK/StringBuilder.h>
#include <AK/Types.h>
#include <AK/URL.h>
#include <Applications/CrashReporter/CrashReporterWindowGML.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibCoredump/Backtrace.h>
#include <LibCoredump/Reader.h>
#include <LibDesktop/AppFile.h>
#include <LibDesktop/Launcher.h>
#include <LibELF/Core.h>
#include <LibGUI/Application.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/FileIconProvider.h>
#include <LibGUI/Icon.h>
#include <LibGUI/ImageWidget.h>
#include <LibGUI/Label.h>
#include <LibGUI/LinkLabel.h>
#include <LibGUI/Progressbar.h>
#include <LibGUI/TabWidget.h>
#include <LibGUI/TextEditor.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Window.h>
#include <string.h>
#include <unistd.h>

struct TitleAndText {
    String title;
    String text;
};

static NonnullRefPtr<GUI::Window> create_progress_window()
{
    auto window = GUI::Window::construct();
    window->set_title("CrashReporter");
    window->set_resizable(false);
    window->resize(240, 64);
    window->center_on_screen();
    auto& main_widget = window->set_main_widget<GUI::Widget>();
    main_widget.set_fill_with_background_color(true);
    main_widget.set_layout<GUI::VerticalBoxLayout>();
    auto& label = main_widget.add<GUI::Label>("Generating crash report...");
    label.set_fixed_height(30);
    auto& progressbar = main_widget.add<GUI::Progressbar>();
    progressbar.set_name("progressbar");
    progressbar.set_fixed_width(150);
    progressbar.set_fixed_height(22);
    return window;
}

static TitleAndText build_backtrace(Coredump::Reader const& coredump, ELF::Core::ThreadInfo const& thread_info, size_t thread_index)
{
    // Show a very simple progress window ASAP to make crashing feel more responsive.
    // FIXME: This is not the most beautifully factored thing.
    auto progress_window = create_progress_window();
    progress_window->show();

    auto& progressbar = *progress_window->main_widget()->find_descendant_of_type_named<GUI::Progressbar>("progressbar");

    auto timer = Core::ElapsedTimer::start_new();
    Coredump::Backtrace backtrace(coredump, thread_info, [&](size_t frame_index, size_t frame_count) {
        progress_window->set_progress(100.0f * (float)(frame_index + 1) / (float)frame_count);
        progressbar.set_value(frame_index + 1);
        progressbar.set_max(frame_count);
        Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
    });
    progress_window->close();

    auto metadata = coredump.metadata();

    dbgln("Generating backtrace took {} ms", timer.elapsed());

    StringBuilder builder;

    auto prepend_metadata = [&](auto& key, StringView fmt) {
        auto maybe_value = metadata.get(key);
        if (!maybe_value.has_value() || maybe_value.value().is_empty())
            return;
        builder.appendff(fmt, maybe_value.value());
        builder.append('\n');
        builder.append('\n');
    };

    if (metadata.contains("assertion"))
        prepend_metadata("assertion", "ASSERTION FAILED: {}");
    else if (metadata.contains("pledge_violation"))
        prepend_metadata("pledge_violation", "Has not pledged {}");

    auto fault_address = metadata.get("fault_address");
    auto fault_type = metadata.get("fault_type");
    auto fault_access = metadata.get("fault_access");
    if (fault_address.has_value() && fault_type.has_value() && fault_access.has_value()) {
        builder.appendff("{} fault on {} at address {}\n\n", fault_type.value(), fault_access.value(), fault_address.value());
    }

    auto first_entry = true;
    for (auto& entry : backtrace.entries()) {
        if (first_entry)
            first_entry = false;
        else
            builder.append('\n');
        builder.append(entry.to_string());
    }

    dbgln("--- Backtrace for thread #{} (TID {}) ---", thread_index, thread_info.tid);
    for (auto& entry : backtrace.entries()) {
        dbgln("{}", entry.to_string(true));
    }

    return {
        String::formatted("Thread #{} (TID {})", thread_index, thread_info.tid),
        builder.build()
    };
}

static TitleAndText build_cpu_registers(const ELF::Core::ThreadInfo& thread_info, size_t thread_index)
{
    auto& regs = thread_info.regs;

    StringBuilder builder;

#if ARCH(I386)
    builder.appendff("eax={:p} ebx={:p} ecx={:p} edx={:p}\n", regs.eax, regs.ebx, regs.ecx, regs.edx);
    builder.appendff("ebp={:p} esp={:p} esi={:p} edi={:p}\n", regs.ebp, regs.esp, regs.esi, regs.edi);
    builder.appendff("eip={:p} eflags={:p}", regs.eip, regs.eflags);
#else
    builder.appendff("rax={:p} rbx={:p} rcx={:p} rdx={:p}\n", regs.rax, regs.rbx, regs.rcx, regs.rdx);
    builder.appendff("rbp={:p} rsp={:p} rsi={:p} rdi={:p}\n", regs.rbp, regs.rsp, regs.rsi, regs.rdi);
    builder.appendff(" r8={:p}  r9={:p} r10={:p} r11={:p}\n", regs.r8, regs.r9, regs.r10, regs.r11);
    builder.appendff("r12={:p} r13={:p} r14={:p} r15={:p}\n", regs.r12, regs.r13, regs.r14, regs.r15);
    builder.appendff("rip={:p} rflags={:p}", regs.rip, regs.rflags);
#endif

    return {
        String::formatted("Thread #{} (TID {})", thread_index, thread_info.tid),
        builder.build()
    };
}

int main(int argc, char** argv)
{
    if (pledge("stdio recvfd sendfd cpath rpath unix", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    auto app = GUI::Application::construct(argc, argv);

    const char* coredump_path = nullptr;
    bool unlink_after_use = false;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("Show information from an application crash coredump.");
    args_parser.add_positional_argument(coredump_path, "Coredump path", "coredump-path");
    args_parser.add_option(unlink_after_use, "Delete the coredump after its parsed", "unlink", 0);
    args_parser.parse(argc, argv);

    Vector<TitleAndText> thread_backtraces;
    Vector<TitleAndText> thread_cpu_registers;

    String executable_path;
    Vector<String> arguments;
    Vector<String> environment;
    int pid { 0 };
    u8 termination_signal { 0 };

    {
        auto coredump = Coredump::Reader::create(coredump_path);
        if (!coredump) {
            warnln("Could not open coredump '{}'", coredump_path);
            return 1;
        }

        size_t thread_index = 0;
        coredump->for_each_thread_info([&](auto& thread_info) {
            thread_backtraces.append(build_backtrace(*coredump, thread_info, thread_index));
            thread_cpu_registers.append(build_cpu_registers(thread_info, thread_index));
            ++thread_index;
            return IterationDecision::Continue;
        });

        executable_path = coredump->process_executable_path();
        arguments = coredump->process_arguments();
        environment = coredump->process_environment();
        pid = coredump->process_pid();
        termination_signal = coredump->process_termination_signal();
    }

    if (unlink_after_use) {
        if (Core::File::remove(coredump_path, Core::File::RecursionMode::Disallowed, false).is_error())
            dbgln("Failed deleting coredump file");
    }

    if (pledge("stdio recvfd sendfd rpath unix", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    if (unveil(executable_path.characters(), "r") < 0) {
        perror("unveil");
        return 1;
    }

    if (unveil("/res", "r") < 0) {
        perror("unveil");
        return 1;
    }

    if (unveil("/tmp/portal/launch", "rw") < 0) {
        perror("unveil");
        return 1;
    }

    if (unveil(nullptr, nullptr) < 0) {
        perror("unveil");
        return 1;
    }

    auto app_icon = GUI::Icon::default_icon("app-crash-reporter");

    auto window = GUI::Window::construct();
    window->set_title("Crash Reporter");
    window->set_icon(app_icon.bitmap_for_size(16));
    window->resize(460, 340);
    window->center_on_screen();

    auto& widget = window->set_main_widget<GUI::Widget>();
    widget.load_from_gml(crash_reporter_window_gml);

    auto& icon_image_widget = *widget.find_descendant_of_type_named<GUI::ImageWidget>("icon");
    icon_image_widget.set_bitmap(GUI::FileIconProvider::icon_for_executable(executable_path).bitmap_for_size(32));

    auto app_name = LexicalPath::basename(executable_path);
    auto af = Desktop::AppFile::get_for_app(app_name);
    if (af->is_valid())
        app_name = af->name();

    auto& description_label = *widget.find_descendant_of_type_named<GUI::Label>("description");
    description_label.set_text(String::formatted("\"{}\" (PID {}) has crashed - {} (signal {})", app_name, pid, strsignal(termination_signal), termination_signal));

    auto& executable_link_label = *widget.find_descendant_of_type_named<GUI::LinkLabel>("executable_link");
    executable_link_label.set_text(LexicalPath::canonicalized_path(executable_path));
    executable_link_label.on_click = [&] {
        LexicalPath path { executable_path };
        Desktop::Launcher::open(URL::create_with_file_protocol(path.dirname(), path.basename()));
    };

    auto& coredump_link_label = *widget.find_descendant_of_type_named<GUI::LinkLabel>("coredump_link");
    coredump_link_label.set_text(LexicalPath::canonicalized_path(coredump_path));
    coredump_link_label.on_click = [&] {
        LexicalPath path { coredump_path };
        Desktop::Launcher::open(URL::create_with_file_protocol(path.dirname(), path.basename()));
    };

    auto& arguments_label = *widget.find_descendant_of_type_named<GUI::Label>("arguments_label");
    arguments_label.set_text(String::join(" ", arguments));

    auto& tab_widget = *widget.find_descendant_of_type_named<GUI::TabWidget>("tab_widget");

    auto& backtrace_tab = tab_widget.add_tab<GUI::Widget>("Backtrace");
    backtrace_tab.set_layout<GUI::VerticalBoxLayout>();
    backtrace_tab.layout()->set_margins(4);

    auto& backtrace_label = backtrace_tab.add<GUI::Label>("A backtrace for each thread alive during the crash is listed below:");
    backtrace_label.set_text_alignment(Gfx::TextAlignment::CenterLeft);
    backtrace_label.set_fixed_height(16);

    auto& backtrace_tab_widget = backtrace_tab.add<GUI::TabWidget>();
    backtrace_tab_widget.set_tab_position(GUI::TabWidget::TabPosition::Bottom);
    for (auto& backtrace : thread_backtraces) {
        auto& container = backtrace_tab_widget.add_tab<GUI::Widget>(backtrace.title);
        container.set_layout<GUI::VerticalBoxLayout>();
        container.layout()->set_margins(4);
        auto& backtrace_text_editor = container.add<GUI::TextEditor>();
        backtrace_text_editor.set_text(backtrace.text);
        backtrace_text_editor.set_mode(GUI::TextEditor::Mode::ReadOnly);
        backtrace_text_editor.set_should_hide_unnecessary_scrollbars(true);
    }

    auto& cpu_registers_tab = tab_widget.add_tab<GUI::Widget>("CPU Registers");
    cpu_registers_tab.set_layout<GUI::VerticalBoxLayout>();
    cpu_registers_tab.layout()->set_margins(4);

    auto& cpu_registers_label = cpu_registers_tab.add<GUI::Label>("The CPU register state for each thread alive during the crash is listed below:");
    cpu_registers_label.set_text_alignment(Gfx::TextAlignment::CenterLeft);
    cpu_registers_label.set_fixed_height(16);

    auto& cpu_registers_tab_widget = cpu_registers_tab.add<GUI::TabWidget>();
    cpu_registers_tab_widget.set_tab_position(GUI::TabWidget::TabPosition::Bottom);
    for (auto& cpu_registers : thread_cpu_registers) {
        auto& container = cpu_registers_tab_widget.add_tab<GUI::Widget>(cpu_registers.title);
        container.set_layout<GUI::VerticalBoxLayout>();
        container.layout()->set_margins(4);
        auto& cpu_registers_text_editor = container.add<GUI::TextEditor>();
        cpu_registers_text_editor.set_text(cpu_registers.text);
        cpu_registers_text_editor.set_mode(GUI::TextEditor::Mode::ReadOnly);
        cpu_registers_text_editor.set_should_hide_unnecessary_scrollbars(true);
    }

    auto& environment_tab = tab_widget.add_tab<GUI::Widget>("Environment");
    environment_tab.set_layout<GUI::VerticalBoxLayout>();
    environment_tab.layout()->set_margins(4);

    auto& environment_text_editor = environment_tab.add<GUI::TextEditor>();
    environment_text_editor.set_text(String::join("\n", environment));
    environment_text_editor.set_mode(GUI::TextEditor::Mode::ReadOnly);
    environment_text_editor.set_should_hide_unnecessary_scrollbars(true);

    auto& close_button = *widget.find_descendant_of_type_named<GUI::Button>("close_button");
    close_button.on_click = [&](auto) {
        app->quit();
    };

    window->show();

    return app->exec();
}
