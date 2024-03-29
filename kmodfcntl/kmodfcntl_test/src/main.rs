use nix::fcntl::FcntlArg::F_SETFL;
use nix::fcntl::OFlag;
use std::io::Read;
use std::os::fd::AsRawFd;

const KMOD_FCNTL_IOCTL_BASE: u8 = b'L';
const KMOD_FCNTL_IOCTL_PAUSE_PRODUCING: u8 = 0x25;

nix::ioctl_write_int!(
    pause_producing,
    KMOD_FCNTL_IOCTL_BASE,
    KMOD_FCNTL_IOCTL_PAUSE_PRODUCING
);

fn main() {
    let mut f = std::fs::File::open("/dev/kmod_fcntl").unwrap();

    // Resume producing
    unsafe {
        pause_producing(f.as_raw_fd(), 0).unwrap();
    }

    let mut buf = [0_u8; 1];

    // Blocking reads, the message is produced once per 2 seconds.
    for i in 0..5 {
        println!("--> Blocking read started #{i}");

        let r = f.read_exact(&mut buf);
        println!("<-- Blocking read finished #{i}, result {r:?}\n");
    }

    // Let a message be produced
    std::thread::sleep(std::time::Duration::from_secs(3));

    // Pause producing
    unsafe {
        pause_producing(f.as_raw_fd(), 1).unwrap();
    }

    // Switch the file descriptor to the non-blocking mode.
    // Need to OR the flag?
    nix::fcntl::fcntl(f.as_raw_fd(), F_SETFL(OFlag::O_NONBLOCK)).unwrap();

    // Excersize non-blocking reads.
    for i in 0..5 {
        println!("--> Non-blocking read started #{i}");

        let r = f.read_exact(&mut buf);
        println!("<-- Non-blocking read finished #{i}, result {r:?}\n");
    }
}
