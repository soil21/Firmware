/****************************************************************************
 *
 *   Copyright (c) 2012-2015 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file i2c.cpp
 *
 * Base class for devices attached via the I2C bus.
 *
 * @todo Bus frequency changes; currently we do nothing with the value
 *       that is supplied.  Should we just depend on the bus knowing?
 */

#include "i2c.h"
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace device
{

I2C::I2C(const char *name,
	 const char *devname,
	 int bus,
	 uint16_t address) :
	// base class
	CDev(name, devname),
	// public
	// protected
	_retries(0),
	// private
	_bus(bus),
	_address(address),
	_fd(-1),
	_dname(devname)
{
	// fill in _device_id fields for a I2C device
	_device_id.devid_s.bus_type = DeviceBusType_I2C;
	_device_id.devid_s.bus = bus;
	_device_id.devid_s.address = address;
	// devtype needs to be filled in by the driver
	_device_id.devid_s.devtype = 0;     
}

I2C::~I2C()
{
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

int
I2C::init()
{
	int ret = PX4_OK;

	// Assume the driver set the desired bus frequency. There is no standard
	// way to set it from user space.

	// do base class init, which will create device node, etc
	ret = CDev::init();

	if (ret != PX4_OK) {
		debug("cdev init failed");
		return ret;
	}

	_fd = ::open(_dname.c_str(), O_RDWR);
        if (_fd < 0) {
                warnx("could not open %s", _dname.c_str());
                return -errno;
        }

	return ret;
}

int
I2C::transfer(const uint8_t *send, unsigned send_len, uint8_t *recv, unsigned recv_len)
{
	struct i2c_msg msgv[2];
	unsigned msgs;
	struct i2c_rdwr_ioctl_data packets;
	int ret;
	unsigned retry_count = 0;

	do {
		//	debug("transfer out %p/%u  in %p/%u", send, send_len, recv, recv_len);
		msgs = 0;

		if (send_len > 0) {
			msgv[msgs].addr = _address;
			msgv[msgs].flags = 0;
			msgv[msgs].buf = const_cast<uint8_t *>(send);
			msgv[msgs].len = send_len;
			msgs++;
		}

		if (recv_len > 0) {
			msgv[msgs].addr = _address;
			msgv[msgs].flags = I2C_M_READ;
			msgv[msgs].buf = recv;
			msgv[msgs].len = recv_len;
			msgs++;
		}

		if (msgs == 0)
			return -EINVAL;

		packets.msgs  = msgv;
		packets.nmsgs = msgs;

		ret = ::ioctl(_fd, I2C_RDWR, &packets);
		if (ret < 0) {
        		warnx("I2C transfer failed");
        		return 1;
    		}

		/* success */
		if (ret == PX4_OK)
			break;

// No way to reset device from userspace
#if 0
		/* if we have already retried once, or we are going to give up, then reset the bus */
		if ((retry_count >= 1) || (retry_count >= _retries))
			px4_i2creset(_dev);
#endif

	} while (retry_count++ < _retries);

	return ret;
}

int
I2C::transfer(struct i2c_msg *msgv, unsigned msgs)
{
	struct i2c_rdwr_ioctl_data packets;
	int ret;
	unsigned retry_count = 0;

	/* force the device address into the message vector */
	for (unsigned i = 0; i < msgs; i++)
		msgv[i].addr = _address;

	do {
		packets.msgs  = msgv;
		packets.nmsgs = msgs;

		ret = ::ioctl(_fd, I2C_RDWR, &packets);
		if (ret < 0) {
        		warnx("I2C transfer failed");
        		return 1;
    		}

		/* success */
		if (ret == PX4_OK)
			break;

// No way to reset device from userspace
#if 0
		/* if we have already retried once, or we are going to give up, then reset the bus */
		if ((retry_count >= 1) || (retry_count >= _retries))
			px4_i2creset(_dev);
#endif

	} while (retry_count++ < _retries);

	return ret;
}

} // namespace device
