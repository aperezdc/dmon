// dmon.rs
// Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
// Distributed under terms of the MIT license.

#![feature(plugin)]
#![plugin(clippy)]

extern crate libc;


#[repr(C)]
pub enum Action {
    None = 0,
    Start,
    Stop,
    Signal,
}

#[repr(C)]
pub struct Task {
    // w_obj_t crap
    __refs: libc::size_t,
    __dtor: *mut libc::c_void,

    // task_t members.
    pid: libc::pid_t,
    action: Action,
    argc: libc::c_int,
    argv: *mut *mut libc::c_char,
    write_fd: libc::c_int,
    read_fd: libc::c_int,
    signal: libc::c_int,
    started: libc::time_t, // TODO: Missing items.
}


const FORWARD_SIGNALS: [libc::c_int; 6] =
    [libc::SIGCONT, libc::SIGALRM, libc::SIGQUIT, libc::SIGUSR1, libc::SIGUSR2, libc::SIGHUP];

const NO_PID: libc::pid_t = -1;


fn log_enabled() -> bool {
    unsafe { *log_fds != -1 }
}

#[no_mangle]
pub extern "C" fn almost_zerof(v: libc::c_double) -> bool {
    v < 0.000000001
}

extern "C" {
    fn task_signal_dispatch(task: &mut Task);
    fn task_action(task: &mut Task, action: Action);
    static log_fds: *mut libc::c_int;
    static mut cmd_task: Task;
    static cmd_timeout: libc::c_ulong;
    static cmd_signals: bool;
    static mut log_task: Task;
    static log_signals: bool;
    static mut check_child: libc::c_int;
    static mut running: libc::c_int;
    static success_exit: bool;
}


#[no_mangle]
pub extern "C" fn handle_signal(signum: libc::c_int) {
    // TODO: Debug statements.
    // W_DEBUG ("handle signal $i ($s)\n", signum, signal_to_name (signum));

    match signum {
        libc::SIGINT | libc::SIGTERM => {
            // Stop gracefuly.
            unsafe { running = 0 }
            return;
        }
        libc::SIGCHLD => {
            unsafe { check_child = 1 }
            return;
        }
        libc::SIGALRM if unsafe { cmd_timeout } > 0 => {
            // TODO:
            // write_status ("cmd timeout $L\n", (unsigned long) cmd_task.pid);
            unsafe {
                task_action(&mut cmd_task, Action::Stop);
                task_action_queue(&mut cmd_task, Action::Start);
                libc::alarm(cmd_timeout as libc::c_uint);
            }
            return;
        }
        _ => {}
    }

    if FORWARD_SIGNALS.iter().any(|&x| x == signum) {
        // Try to forward signals.
        if unsafe { cmd_signals } {
            // TODO: W_DEBUGC ("  delayed signal $i for cmd process\n", signum);
            unsafe {
                task_action_queue(&mut cmd_task, Action::Signal);
                task_signal_queue(&mut cmd_task, signum);
            }
        }
        if unsafe { log_signals } && log_enabled() {
            // TODO: W_DEBUGC ("  delayed signal $i for log process\n", signum);
            unsafe {
                task_action_queue(&mut log_task, Action::Signal);
                task_signal_queue(&mut log_task, signum);
            }
        }
    }
}


#[no_mangle]
pub extern "C" fn reap_and_check() -> libc::c_int {
    // TODO: W_DEBUG ("waiting for a children to reap...\n");
    let mut status = 0 as libc::c_int;
    let pid = unsafe { libc::waitpid(-1 as libc::c_int, &mut status, libc::WNOHANG) };
    if pid == unsafe { cmd_task.pid } {
        // TODO: W_DEBUGC ("  reaped cmd process $I\n", (unsigned) pid);

        // TODO: write_status ("cmd exit $L $i\n", (unsigned long) pid, status);
        unsafe {
            cmd_task.pid = NO_PID;

            // If exit-on-success was request AND the process exited ok,
            // then we do not want to respawn, but to gracefully shutdown.
            //
            if success_exit && libc::WIFEXITED(status) && libc::WEXITSTATUS(status) == 0 {
                // TODO: W_DEBUGC ("  cmd process ended successfully, will exit\n");
                running = 0;
            } else {
                task_action_queue(&mut cmd_task, Action::Start);
            }
        }
        return status;
    } else if log_enabled() && pid == unsafe { log_task.pid } {
        // TODO: W_DEBUGC ("  reaped log process $I\n", (unsigned) pid);

        // TODO: write_status ("log exit $L $i\n", (unsigned long) pid, status);
        unsafe {
            log_task.pid = NO_PID;
            task_action_queue(&mut log_task, Action::Start);
        }
    } else {
        // TODO: W_DEBUGC ("  reaped unknown process $I", (unsigned) pid);
    }

    // For cases where a return status is not meaningful (PIDs other than
    // that of the command being run) just return some invalid return code
    // value.
    //
    -1
}

#[no_mangle]
pub extern "C" fn task_action_queue(task: &mut Task, action: Action) {
    task.action = action;
}

#[no_mangle]
pub extern "C" fn task_signal_queue(task: &mut Task, signum: libc::c_int) {
    task.signal = signum;
}

#[no_mangle]
pub extern "C" fn task_signal(task: &mut Task, signum: libc::c_int) {
    unsafe {
        // Dispatch pending signal first if needed.
        task_signal_dispatch(task);
    }

    // Then send our own.
    task_signal_queue(task, signum);
    unsafe {
        task_signal_dispatch(task);
    }
}


#[no_mangle]
#[allow(not_unsafe_ptr_arg_deref)]
pub extern "C" fn name_to_gid(name: *const libc::c_char,
                              ogid: *mut libc::gid_t) -> libc::c_int
{
    let s = unsafe {
        assert!(!name.is_null());
        std::ffi::CStr::from_ptr(name)
    }.to_str().unwrap();

    let gid = {
        if let Ok(gid) = s.trim().parse::<u32>() {
            let g = unsafe { libc::getgrgid(gid as libc::gid_t) };
            if g.is_null() {
                None
            } else {
                Some(gid as libc::gid_t)
            }
        } else {
            let g = unsafe { libc::getgrnam(name) };
            if g.is_null() {
                None
            } else {
                Some(unsafe { (*g).gr_gid })
            }
        }
    };

    match gid {
        Some(gid) => {
            if !ogid.is_null() {
                unsafe { *ogid = gid };
            }
            0
        },
        None => 1
    }
}


#[no_mangle]
#[allow(not_unsafe_ptr_arg_deref)]
pub extern "C" fn name_to_uidgid(name: *const libc::c_char,
                                 ouid: *mut libc::uid_t,
                                 ogid: *mut libc::gid_t) -> libc::c_int
{
    let s = unsafe {
        assert!(!name.is_null());
        std::ffi::CStr::from_ptr(name)
    }.to_str().unwrap();

    let u = {
        if let Ok(uid) = s.trim().parse::<u32>() {
            let uu = unsafe { libc::getpwuid(uid as libc::uid_t) };
            if uu.is_null() {
                None
            } else {
                Some(unsafe { *uu })
            }
        } else {
            let uu = unsafe { libc::getpwnam(name) };
            if uu.is_null() {
                None
            } else {
                Some(unsafe { *uu })
            }
        }
    };

    match u {
        Some(uu) => {
            if !ouid.is_null() {
                unsafe { *ouid = uu.pw_uid };
            }
            if !ogid.is_null() {
                unsafe { *ogid = uu.pw_gid };
            }
            0
        },
        None => 1
    }
}
