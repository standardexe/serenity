/*
 * Copyright (c) 2020, Itamar S. <itamar8910@gmail.com>
 * Copyright (c) 2020-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ScopeGuard.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/PrivateInodeVMObject.h>
#include <Kernel/Memory/Region.h>
#include <Kernel/Memory/ScopedAddressSpaceSwitcher.h>
#include <Kernel/Memory/SharedInodeVMObject.h>
#include <Kernel/Process.h>
#include <Kernel/ThreadTracer.h>

namespace Kernel {

static KResultOr<u32> handle_ptrace(const Kernel::Syscall::SC_ptrace_params& params, Process& caller)
{
    SpinlockLocker scheduler_lock(g_scheduler_lock);
    if (params.request == PT_TRACE_ME) {
        if (Process::current().tracer())
            return EBUSY;

        caller.set_wait_for_tracer_at_next_execve(true);
        return KSuccess;
    }

    // FIXME: PID/TID BUG
    // This bug allows to request PT_ATTACH (or anything else) the same process, as
    // long it is not the main thread. Alternatively, if this is desired, then the
    // bug is that this prevents PT_ATTACH to the main thread from another thread.
    if (params.tid == caller.pid().value())
        return EINVAL;

    auto peer = Thread::from_tid(params.tid);
    if (!peer)
        return ESRCH;

    MutexLocker ptrace_locker(peer->process().ptrace_lock());

    if ((peer->process().uid() != caller.euid())
        || (peer->process().uid() != peer->process().euid())) // Disallow tracing setuid processes
        return EACCES;

    if (!peer->process().is_dumpable())
        return EACCES;

    auto& peer_process = peer->process();
    if (params.request == PT_ATTACH) {
        if (peer_process.tracer()) {
            return EBUSY;
        }
        TRY(peer_process.start_tracing_from(caller.pid()));
        SpinlockLocker lock(peer->get_lock());
        if (peer->state() != Thread::State::Stopped) {
            peer->send_signal(SIGSTOP, &caller);
        }
        return KSuccess;
    }

    auto* tracer = peer_process.tracer();

    if (!tracer)
        return EPERM;

    if (tracer->tracer_pid() != caller.pid())
        return EBUSY;

    if (peer->state() == Thread::State::Running)
        return EBUSY;

    scheduler_lock.unlock();

    switch (params.request) {
    case PT_CONTINUE:
        peer->send_signal(SIGCONT, &caller);
        break;

    case PT_DETACH:
        peer_process.stop_tracing();
        peer->send_signal(SIGCONT, &caller);
        break;

    case PT_SYSCALL:
        tracer->set_trace_syscalls(true);
        peer->send_signal(SIGCONT, &caller);
        break;

    case PT_GETREGS: {
        if (!tracer->has_regs())
            return EINVAL;
        auto* regs = reinterpret_cast<PtraceRegisters*>(params.addr);
        TRY(copy_to_user(regs, &tracer->regs()));
        break;
    }

    case PT_SETREGS: {
        if (!tracer->has_regs())
            return EINVAL;

        PtraceRegisters regs {};
        TRY(copy_from_user(&regs, (const PtraceRegisters*)params.addr));

        auto& peer_saved_registers = peer->get_register_dump_from_stack();
        // Verify that the saved registers are in usermode context
        if ((peer_saved_registers.cs & 0x03) != 3)
            return EFAULT;

        tracer->set_regs(regs);
        copy_ptrace_registers_into_kernel_registers(peer_saved_registers, regs);
        break;
    }

    case PT_PEEK: {
        Kernel::Syscall::SC_ptrace_peek_params peek_params {};
        TRY(copy_from_user(&peek_params, reinterpret_cast<Kernel::Syscall::SC_ptrace_peek_params*>(params.addr)));
        if (!Memory::is_user_address(VirtualAddress { peek_params.address }))
            return EFAULT;
        auto data = TRY(peer->process().peek_user_data(Userspace<const u32*> { (FlatPtr)peek_params.address }));
        TRY(copy_to_user(peek_params.out_data, &data));
        break;
    }

    case PT_POKE:
        if (!Memory::is_user_address(VirtualAddress { params.addr }))
            return EFAULT;
        return peer->process().poke_user_data(Userspace<u32*> { (FlatPtr)params.addr }, params.data);

    case PT_PEEKDEBUG: {
        Kernel::Syscall::SC_ptrace_peek_params peek_params {};
        TRY(copy_from_user(&peek_params, reinterpret_cast<Kernel::Syscall::SC_ptrace_peek_params*>(params.addr)));
        auto data = TRY(peer->peek_debug_register(reinterpret_cast<uintptr_t>(peek_params.address)));
        TRY(copy_to_user(peek_params.out_data, &data));
        break;
    }
    case PT_POKEDEBUG:
        return peer->poke_debug_register(reinterpret_cast<uintptr_t>(params.addr), params.data);
    default:
        return EINVAL;
    }

    return KSuccess;
}

KResultOr<FlatPtr> Process::sys$ptrace(Userspace<const Syscall::SC_ptrace_params*> user_params)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this)
    REQUIRE_PROMISE(ptrace);
    auto params = TRY(copy_typed_from_user(user_params));

    auto result = handle_ptrace(params, *this);
    return result.is_error() ? result.error().error() : result.value();
}

/**
 * "Does this process have a thread that is currently being traced by the provided process?"
 */
bool Process::has_tracee_thread(ProcessID tracer_pid)
{
    if (auto tracer = this->tracer())
        return tracer->tracer_pid() == tracer_pid;
    return false;
}

KResultOr<u32> Process::peek_user_data(Userspace<const u32*> address)
{
    // This function can be called from the context of another
    // process that called PT_PEEK
    ScopedAddressSpaceSwitcher switcher(*this);
    uint32_t data;
    TRY(copy_from_user(&data, address));
    return data;
}

KResult Process::poke_user_data(Userspace<u32*> address, u32 data)
{
    Memory::VirtualRange range = { VirtualAddress(address), sizeof(u32) };
    auto* region = address_space().find_region_containing(range);
    if (!region)
        return EFAULT;
    ScopedAddressSpaceSwitcher switcher(*this);
    if (region->is_shared()) {
        // If the region is shared, we change its vmobject to a PrivateInodeVMObject
        // to prevent the write operation from changing any shared inode data
        VERIFY(region->vmobject().is_shared_inode());
        auto vmobject = TRY(Memory::PrivateInodeVMObject::try_create_with_inode(static_cast<Memory::SharedInodeVMObject&>(region->vmobject()).inode()));
        region->set_vmobject(move(vmobject));
        region->set_shared(false);
    }
    const bool was_writable = region->is_writable();
    if (!was_writable) {
        region->set_writable(true);
        region->remap();
    }
    ScopeGuard rollback([&]() {
        if (!was_writable) {
            region->set_writable(false);
            region->remap();
        }
    });

    return copy_to_user(address, &data);
}

KResultOr<u32> Thread::peek_debug_register(u32 register_index)
{
    u32 data;
    switch (register_index) {
    case 0:
        data = m_debug_register_state.dr0;
        break;
    case 1:
        data = m_debug_register_state.dr1;
        break;
    case 2:
        data = m_debug_register_state.dr2;
        break;
    case 3:
        data = m_debug_register_state.dr3;
        break;
    case 6:
        data = m_debug_register_state.dr6;
        break;
    case 7:
        data = m_debug_register_state.dr7;
        break;
    default:
        return EINVAL;
    }
    return data;
}

KResult Thread::poke_debug_register(u32 register_index, u32 data)
{
    switch (register_index) {
    case 0:
        m_debug_register_state.dr0 = data;
        break;
    case 1:
        m_debug_register_state.dr1 = data;
        break;
    case 2:
        m_debug_register_state.dr2 = data;
        break;
    case 3:
        m_debug_register_state.dr3 = data;
        break;
    case 7:
        m_debug_register_state.dr7 = data;
        break;
    default:
        return EINVAL;
    }
    return KSuccess;
}

}
