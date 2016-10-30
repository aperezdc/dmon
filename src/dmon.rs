//
// libdmon.rs
// Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
// Distributed under terms of the MIT license.
//

extern crate libc;

extern {
	fn task_signal_dispatch(task: &mut Task);
}


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
	started: libc::time_t,
	// TODO: Missing items.
}


#[no_mangle]
pub extern fn task_action_queue(task: &mut Task, action: Action)
{
	task.action = action;
}

#[no_mangle]
pub extern fn task_signal_queue(task: &mut Task, signum: libc::c_int)
{
	task.signal = signum;
}

#[no_mangle]
pub extern fn task_signal(task: &mut Task, signum: libc::c_int)
{
	unsafe {
		// Dispatch pending signal first if needed.
		task_signal_dispatch (task);
	}

	// Then send our own.
	task_signal_queue (task, signum);
	unsafe {
		task_signal_dispatch (task);
	}
}
