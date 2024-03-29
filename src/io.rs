use core::fmt;
use limine::LimineTerminalRequest;
use spin::Mutex;
use x86_64;

static TERMINAL_REQUEST: LimineTerminalRequest = LimineTerminalRequest::new(0);

struct Writer {
    terminals: Option<&'static limine::LimineTerminalResponse>,
}

unsafe impl Send for Writer {}

impl fmt::Write for Writer {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        // Get the Terminal response and cache it.
        let response = match self.terminals {
            None => {
                let response = TERMINAL_REQUEST.get_response().get().ok_or(fmt::Error)?;
                self.terminals = Some(response);
                response
            }
            Some(resp) => resp,
        };

        let write = response.write().ok_or(fmt::Error)?;

        // Output the string onto each terminal.
        for terminal in response.terminals() {
            write(terminal, s);
        }

        Ok(())
    }
}

static WRITER: Mutex<Writer> = Mutex::new(Writer { terminals: None });

pub fn _print(args: fmt::Arguments) {
    // NOTE: Locking needs to happen around `print_fmt`, not `print_str`, as the former
    // will call the latter potentially multiple times per invocation.

    x86_64::instructions::interrupts::without_interrupts(|| {
        let mut writer = WRITER.lock();
        fmt::Write::write_fmt(&mut *writer, args).ok();
    });
}

#[macro_export]
macro_rules! print {
    ($($t:tt)*) => { $crate::io::_print(format_args!($($t)*)) };
}

#[macro_export]
macro_rules! println {
    ()          => { $crate::print!("\n"); };
    // On nightly, `format_args_nl!` could also be used.
    ($($t:tt)*) => { $crate::print!("{}\n", format_args!($($t)*)); };
}
