// System includes
//see https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/ 
//for starting code
#include <stdio.h>   	// Standard input/output definitions
#include <string.h>  	// String function definitions
#include <unistd.h>  	// UNIX standard function definitions
#include <fcntl.h>   	// File control definitions
#include <errno.h>   	// Error number definitions
#include <system_error>	// For throwing std::system_error
#include <sys/ioctl.h> // Used for TCGETS2, which is required for custom baud rates
#include <asm/ioctls.h>
#include <asm/termbits.h>

#include "serial.h"

SerialPort::SerialPort() {
}

SerialPort::~SerialPort() {
	close();
}

void SerialPort::close() {
	if(_fd != -1) {
		::close(_fd);
		_fd = -1;
	}
}


bool SerialPort::open(const char* device, uint32_t baudrate, int32_t timeout_ms) {

	if(timeout_ms < -1)
		return false;
	if(timeout_ms > 25500)
		return false;
// O_RDONLY for read-only, O_WRONLY for write only, O_RDWR for both read/write access
	// 3rd, optional parameter is mode_t mode
	_fd = ::open(device, O_RDWR);

	// Check status
	if(_fd == -1)
		return false;

	configure(baudrate, timeout_ms);

	return true;
}

void SerialPort::configure(uint32_t baudrate, int32_t timeout_ms)
{
	//================== CONFIGURE ==================//

	termios2 tty;
	ioctl(_fd, TCGETS2, &tty);

	//================= (.c_cflag) ===============//

	tty.c_cflag     &=  ~PARENB;       	// No parity bit is added to the output characters
	tty.c_cflag     &=  ~CSTOPB;		// Only one stop-bit is used
	tty.c_cflag     &=  ~CSIZE;			// CSIZE is a mask for the number of bits per character
	tty.c_cflag     |=  CS8;			// Set to 8 bits per character
	tty.c_cflag     &=  ~CRTSCTS;       // Disable hadrware flow control (RTS/CTS)
	tty.c_cflag     |=  CREAD | CLOCAL;     				// Turn on READ & ignore ctrl lines (CLOCAL = 1)

	// This does no different than STANDARD atm, but let's keep
	// them separate for now....
	{ 
		tty.c_cflag &= ~CBAUD;
		tty.c_cflag |= CBAUDEX;
		tty.c_ispeed = baudrate;
		tty.c_ospeed = baudrate;
	}

	//===================== (.c_oflag) =================//

	tty.c_oflag     =   0;              // No remapping, no delays
	tty.c_oflag     &=  ~OPOST;			// Make raw

	//================= CONTROL CHARACTERS (.c_cc[]) ==================//

	// c_cc[VTIME] sets the inter-character timer, in units of 0.1s.
	// Only meaningful when port is set to non-canonical mode
	// VMIN = 0, VTIME = 0: No blocking, return immediately with what is available
	// VMIN > 0, VTIME = 0: read() waits for VMIN bytes, could block indefinitely
	// VMIN = 0, VTIME > 0: Block until any amount of data is available, OR timeout occurs
	// VMIN > 0, VTIME > 0: Block until either VMIN characters have been received, or VTIME
	//                      after first character has elapsed
	// c_cc[WMIN] sets the number of characters to block (wait) for when read() is called.
	// Set to 0 if you don't want read to block. Only meaningful when port set to non-canonical mode

	if(timeout_ms == -1) {
		// Always wait for at least one byte, this could
		// block indefinitely
		tty.c_cc[VTIME] = 0;
		tty.c_cc[VMIN] = 1;
	} else if(timeout_ms == 0) {
		// Setting both to 0 will give a non-blocking read
		tty.c_cc[VTIME] = 0;
		tty.c_cc[VMIN] = 0;
	} else if(timeout_ms > 0) {
		tty.c_cc[VTIME] = (cc_t)(timeout_ms/100);
		tty.c_cc[VMIN] = 0;
	}

	//======================== (.c_iflag) ====================//

	tty.c_iflag     &= ~(IXON | IXOFF | IXANY);			// Turn off s/w flow ctrl
	tty.c_iflag 	&= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);

	//=========================== LOCAL MODES (c_lflag) =======================//

	// Canonical input is when read waits for EOL or EOF characters before returning. In non-canonical mode, the rate at which
	// read() returns is instead controlled by c_cc[VMIN] and c_cc[VTIME]
	tty.c_lflag		&= ~ICANON;								// Turn off canonical input, which is suitable for pass-through
	tty.c_lflag 	&= ~(ECHO);
	tty.c_lflag		&= ~ECHOE;								// Turn off echo erase (echo erase only relevant if canonical input is active)
	tty.c_lflag		&= ~ECHONL;								//
	tty.c_lflag		&= ~ISIG;								// Disables recognition of INTR (interrupt), QUIT and SUSP (suspend) characters


	ioctl(_fd, TCSETS2, &tty);
}

ssize_t SerialPort::write(const void* buf, size_t bytes) {
	if(_fd==-1)
		return -1;
	ssize_t ret = ::write(_fd, buf, bytes);
	if(ret < 0)
		close();
	return ret;
}

ssize_t SerialPort::read(void* buf, size_t bytes) {
	if(_fd==-1)
		return -1;
	ssize_t ret = ::read(_fd, buf, bytes);
	if(ret < 0)
		close();
	return ret;
}
