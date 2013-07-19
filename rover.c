#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "internal/rover.h"



static int do_roverOpenMotor(RoverOutput* _rover,
                             RoverMotor* _motor,
                             const RoverConfigMotor* _config)
{
  int res;

  if (_rover == NULL || _motor == NULL || _config == NULL || _config->m_path == NULL)
    return EINVAL;

  _motor->m_fd = open(_config->m_path, O_WRONLY|O_SYNC, 0);
  if (_motor->m_fd < 0)
  {
    res = errno;
    fprintf(stderr, "open(%s) failed: %d\n", _config->m_path, res);
    _motor->m_fd = -1;
    return res;
  }

  _motor->m_powerBack    = _config->m_powerBack;
  _motor->m_powerNeutral = _config->m_powerNeutral;
  _motor->m_powerForward = _config->m_powerForward;

  return 0;
}

static int do_roverCloseMotor(RoverOutput* _rover,
                              RoverMotor* _motor)
{
  int res;

  if (_rover == NULL || _motor == NULL)
    return EINVAL;

  if (close(_motor->m_fd) != 0)
  {
    res = errno;
    fprintf(stderr, "close() failed: %d\n", res);
    return res;
  }
  _motor->m_fd = -1;

  return 0;
}

static int do_roverOpen(RoverOutput* _rover,
                        const RoverConfig* _config)
{
  int res;

  if (_rover == NULL || _config == NULL)
    return EINVAL;

  if ((res = do_roverOpenMotor(_rover, &_rover->m_motor1, &_config->m_motor1)) != 0)
    return res;

  if ((res = do_roverOpenMotor(_rover, &_rover->m_motor2, &_config->m_motor2)) != 0)
  {
    do_roverCloseMotor(_rover, &_rover->m_motor1);
    return res;
  }

  if ((res = do_roverOpenMotor(_rover, &_rover->m_motor3, &_config->m_motor3)) != 0)
  {
    do_roverCloseMotor(_rover, &_rover->m_motor2);
    do_roverCloseMotor(_rover, &_rover->m_motor1);
    return res;
  }

  if ((res = do_roverOpenMotor(_rover, &_rover->m_motor4, &_config->m_motor4)) != 0)
  {
    do_roverCloseMotor(_rover, &_rover->m_motor3);
    do_roverCloseMotor(_rover, &_rover->m_motor2);
    do_roverCloseMotor(_rover, &_rover->m_motor1);
    return res;
  }

  return 0;
}

static int do_roverClose(RoverOutput* _rover)
{
  if (_rover == NULL)
    return EINVAL;

  do_roverCloseMotor(_rover, &_rover->m_motor4);
  do_roverCloseMotor(_rover, &_rover->m_motor3);
  do_roverCloseMotor(_rover, &_rover->m_motor2);
  do_roverCloseMotor(_rover, &_rover->m_motor1);

  return 0;
}

static int do_roverMotorSetPower(RoverOutput* _rover,
                                 RoverMotor* _motor,
                                 int _power)
{
  int res;

  if (_rover == NULL || _motor == NULL)
    return EINVAL;

  int pwm;

  if (_power == 0)
    pwm = _motor->m_powerNeutral;
  else if (_power < 0)
  {
    if (_power < -100)
      pwm = _motor->m_powerBack;
    else
      pwm = _motor->m_powerNeutral + ((_motor->m_powerBack-_motor->m_powerNeutral)*(-_power))/100;
  }
  else
  {
    if (_power > 100)
      pwm = _motor->m_powerForward;
    else
      pwm = _motor->m_powerNeutral + ((_motor->m_powerForward-_motor->m_powerNeutral)*_power)/100;
  }

  if (dprintf(_motor->m_fd, "%d\n", pwm) < 0)
  {
    res = errno;
    fprintf(stderr, "dprintf(%d) failed: %d\n", pwm, res);
    return res;
  }
  fsync(_motor->m_fd);

  return 0;
}




int roverOutputInit(bool _verbose)
{
  (void)_verbose;
  return 0;
}

int roverOutputFini()
{
  return 0;
}

int roverOutputOpen(RoverOutput* _rover, const RoverConfig* _config)
{
  int res = 0;

  if (_rover == NULL)
    return EINVAL;
  if (_rover->m_opened)
    return EALREADY;

  if ((res = do_roverOpen(_rover, _config)) != 0)
    return res;

  _rover->m_opened = true;

  return 0;
}

int roverOutputClose(RoverOutput* _rover)
{
  if (_rover == NULL)
    return EINVAL;
  if (!_rover->m_opened)
    return EALREADY;

  do_roverClose(_rover);

  _rover->m_opened = false;

  return 0;
}

int roverOutputStart(RoverOutput* _rover)
{
  if (_rover == NULL)
    return EINVAL;
  if (!_rover->m_opened)
    return ENOTCONN;

  do_roverMotorSetPower(_rover, &_rover->m_motor1, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor2, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor3, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor4, 0);

  return 0;
}

int roverOutputStop(RoverOutput* _rover)
{
  if (_rover == NULL)
    return EINVAL;
  if (!_rover->m_opened)
    return ENOTCONN;

  do_roverMotorSetPower(_rover, &_rover->m_motor1, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor2, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor3, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor4, 0);

  return 0;
}

int roverOutputControl(RoverOutput* _rover, int _targetX, int _targetY, int _targetMass)
{
  if (_rover == NULL)
    return EINVAL;
  if (!_rover->m_opened)
    return ENOTCONN;

#warning TEMPORARY!
//  do_roverMotorSetPower(_rover, &_rover->m_motor4, _targetX);
//  do_roverMotorSetPower(_rover, &_rover->m_motor3, _targetY);

  return 0;
}
