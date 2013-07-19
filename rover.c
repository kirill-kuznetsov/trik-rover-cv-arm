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

  if (_power >= -10 && _power <= -10)
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

int do_roverCtrlChasisSetup(RoverOutput* _rover, const RoverConfig* _config)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;

  chasis->m_motorLeft  = &_rover->m_motor1;
  chasis->m_motorRight = &_rover->m_motor2;
  chasis->m_lastSpeed = 0;
  chasis->m_lastYaw = 0;
  chasis->m_zeroX = _config->m_zeroX;
  chasis->m_zeroY = _config->m_zeroY;
  chasis->m_zeroMass = _config->m_zeroMass;

  return 0;
}

int do_roverCtrlHandSetup(RoverOutput* _rover, const RoverConfig* _config)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;

  hand->m_motor      = &_rover->m_motor3;
  hand->m_lastSpeed = 0;
  hand->m_zeroY = _config->m_zeroY;

  return 0;
}

int do_roverCtrlArmSetup(RoverOutput* _rover, const RoverConfig* _config)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

  arm->m_motor      = &_rover->m_motor4;
  arm->m_zeroX = _config->m_zeroX;
  arm->m_zeroY = _config->m_zeroY;
  arm->m_zeroMass = _config->m_zeroMass;

  return 0;
}

int do_roverCtrlChasisStart(RoverOutput* _rover)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;

  chasis->m_lastSpeed = 0;
  chasis->m_lastYaw = 0;

  return 0;
}

int do_roverCtrlHandStart(RoverOutput* _rover)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;

  hand->m_lastSpeed = 0;

  return 0;
}

int do_roverCtrlArmStart(RoverOutput* _rover)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

  (void)arm;

  return 0;
}


int do_roverCtrlChasisPreparing(RoverOutput* _rover)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;

  do_roverMotorSetPower(_rover, chasis->m_motorLeft, 0);
  do_roverMotorSetPower(_rover, chasis->m_motorRight, 0);

  return 0;
}

int do_roverCtrlHandPreparing(RoverOutput* _rover)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;

  do_roverMotorSetPower(_rover, hand->m_motor, 0);

  return 0;
}

int do_roverCtrlArmPreparing(RoverOutput* _rover)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

  do_roverMotorSetPower(_rover, arm->m_motor, -100);

  return 0;
}


int do_roverCtrlChasisSearching(RoverOutput* _rover)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;

  do_roverMotorSetPower(_rover, chasis->m_motorLeft, 0);
  do_roverMotorSetPower(_rover, chasis->m_motorRight, 0);

  return 0;
}

int do_roverCtrlHandSearching(RoverOutput* _rover)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;

  do_roverMotorSetPower(_rover, hand->m_motor, 0);

  return 0;
}

int do_roverCtrlArmSearching(RoverOutput* _rover)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

  do_roverMotorSetPower(_rover, arm->m_motor, 0);

  return 0;
}


int do_roverCtrlChasisTracking(RoverOutput* _rover, int _targetX, int _targetY, int _targetMass)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;
  int yaw;
  int speed;
  int ydiv = 1;

  yaw = _targetX - chasis->m_zeroX;
  if (   (yaw < 0 && chasis->m_lastYaw < 0)
      || (yaw > 0 && chasis->m_lastYaw > 0))
    yaw += chasis->m_lastYaw/5;

  if (_targetMass < chasis->m_zeroMass)
  {
    speed = (100 - (100*_targetMass) / chasis->m_zeroMass);
    if (speed > 50)
      speed = 50 + speed/2;
  }
  else
    speed = (100 - (100*_targetMass) / chasis->m_zeroMass)/2;

  if (   (speed < 0 && chasis->m_lastSpeed < 0)
      || (speed > 0 && chasis->m_lastSpeed > 0))
    speed += chasis->m_lastSpeed/5;

  ydiv = _targetY - chasis->m_zeroY;
  if (ydiv < 0)
    ydiv = -ydiv;
  ydiv = 1 + ydiv/15;

  chasis->m_lastYaw = yaw;
  chasis->m_lastSpeed = speed;

  do_roverMotorSetPower(_rover, chasis->m_motorLeft, (speed+yaw)/ydiv);
  do_roverMotorSetPower(_rover, chasis->m_motorRight, (speed-yaw)/ydiv);

  return 0;
}

int do_roverCtrlHandTracking(RoverOutput* _rover, int _targetX, int _targetY, int _targetMass)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;
  int speed;

  speed = -(_targetY - hand->m_zeroY);
  if (   (speed < 0 && hand->m_lastSpeed < 0)
      || (speed > 0 && hand->m_lastSpeed > 0))
    speed += hand->m_lastSpeed/5;

  hand->m_lastSpeed = speed;

  do_roverMotorSetPower(_rover, hand->m_motor, speed);

  return 0;
}

int do_roverCtrlArmTracking(RoverOutput* _rover, int _targetX, int _targetY, int _targetMass)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

  do_roverMotorSetPower(_rover, arm->m_motor, 0);

  int diffX = _targetX - arm->m_zeroX;
  int diffY = _targetY - arm->m_zeroY;;
  int diffMass = _targetMass - arm->m_zeroMass;
  fprintf(stderr, "%d %d %d  (%d->%d %d->%d %d->%d)\n",
          diffX, diffY, diffMass, _targetX, _targetY, _targetMass, arm->m_zeroX, arm->m_zeroY, arm->m_zeroMass);

  if (diffX <= 10 && diffX >= -10 && diffY <= 10 && diffY >= -10 && diffMass <= 150 && diffMass >= -150)
  {
    fprintf(stderr, "#");
    fflush(stderr);
  }
  else
    _rover->m_stateEntryTime.tv_sec = 0;

  return 0;
}


int do_roverCtrlChasisSqueezing(RoverOutput* _rover)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;

  do_roverMotorSetPower(_rover, chasis->m_motorLeft, 0);
  do_roverMotorSetPower(_rover, chasis->m_motorRight, 0);

  return 0;
}

int do_roverCtrlHandSqueezing(RoverOutput* _rover)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;

  do_roverMotorSetPower(_rover, hand->m_motor, 0);

  return 0;
}

int do_roverCtrlArmSqueezing(RoverOutput* _rover)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

#warning Check lock
  do_roverMotorSetPower(_rover, arm->m_motor, 100);

  return 0;
}


int do_roverCtrlChasisReleasing(RoverOutput* _rover, int _ms)
{
  RoverControlChasis* chasis = &_rover->m_ctrlChasis;

  if (_ms < 2000)
  {
    do_roverMotorSetPower(_rover, chasis->m_motorLeft, 0);
    do_roverMotorSetPower(_rover, chasis->m_motorRight, 0);
  }
  else if (_ms < 4000)
  {
    do_roverMotorSetPower(_rover, chasis->m_motorLeft, 100);
    do_roverMotorSetPower(_rover, chasis->m_motorRight, -100);
  }
  else
  {
    do_roverMotorSetPower(_rover, chasis->m_motorLeft, -100);
    do_roverMotorSetPower(_rover, chasis->m_motorRight, 100);
  }

  return 0;
}

int do_roverCtrlHandReleasing(RoverOutput* _rover, int _ms)
{
  RoverControlHand* hand = &_rover->m_ctrlHand;

  if (_ms < 4000)
    do_roverMotorSetPower(_rover, hand->m_motor, 100);
  else
    do_roverMotorSetPower(_rover, hand->m_motor, -100);

  return 0;
}

int do_roverCtrlArmReleasing(RoverOutput* _rover, int _ms)
{
  RoverControlArm* arm = &_rover->m_ctrlArm;

  if (_ms < 3000)
    do_roverMotorSetPower(_rover, arm->m_motor, 0);
  else
    do_roverMotorSetPower(_rover, arm->m_motor, -100);

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

  do_roverCtrlChasisSetup(_rover, _config);
  do_roverCtrlHandSetup(_rover, _config);
  do_roverCtrlArmSetup(_rover, _config);

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

  _rover->m_state = StatePreparing;
  _rover->m_stateEntryTime.tv_sec = 0;
  _rover->m_stateEntryTime.tv_nsec = 0;

  do_roverMotorSetPower(_rover, &_rover->m_motor1, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor2, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor3, 0);
  do_roverMotorSetPower(_rover, &_rover->m_motor4, 0);

  do_roverCtrlChasisStart(_rover);
  do_roverCtrlHandStart(_rover);
  do_roverCtrlArmStart(_rover);

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

  if (_rover->m_stateEntryTime.tv_sec == 0)
    clock_gettime(CLOCK_MONOTONIC, &_rover->m_stateEntryTime);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  long long msPassed = (now.tv_sec - _rover->m_stateEntryTime.tv_sec) * 1000
                     + (now.tv_nsec - _rover->m_stateEntryTime.tv_nsec) / 1000000;

  switch (_rover->m_state)
  {
    case StatePreparing:
      do_roverCtrlChasisPreparing(_rover);
      do_roverCtrlHandPreparing(_rover);
      do_roverCtrlArmPreparing(_rover);
      if (msPassed > 5000)
      {
        fprintf(stderr, "*** PREPARED, SEARCHING ***\n");
        _rover->m_state = StateSearching;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      break;

    case StateSearching:
      do_roverCtrlChasisSearching(_rover);
      do_roverCtrlHandSearching(_rover);
      do_roverCtrlArmSearching(_rover);
      if (_targetMass > 0)
      {
        fprintf(stderr, "*** FOUND TARGET ***\n");
        _rover->m_state = StateTracking;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      break;

    case StateTracking:
      do_roverCtrlChasisTracking(_rover, _targetX, _targetY, _targetMass);
      do_roverCtrlHandTracking(_rover, _targetX, _targetY, _targetMass);
      do_roverCtrlArmTracking(_rover, _targetX, _targetY, _targetMass);
      if (_targetMass <= 0)
      {
        fprintf(stderr, "*** LOST TARGET ***\n");
        _rover->m_state = StateSearching;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      else if (msPassed > 5000)
      {
        fprintf(stderr, "*** LOCKED TARGET ***\n");
        _rover->m_state = StateSqueezing;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      break;

    case StateSqueezing:
      do_roverCtrlChasisSqueezing(_rover);
      do_roverCtrlHandSqueezing(_rover);
      do_roverCtrlArmSqueezing(_rover);
      if (_targetMass <= 0)
      {
        fprintf(stderr, "*** LOCK FAILED ***\n");
        _rover->m_state = StatePreparing;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      else if (msPassed > 5000)
      {
        fprintf(stderr, "*** RELEASING TARGET ***\n");
        _rover->m_state = StateReleasing;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      break;

    case StateReleasing:
      do_roverCtrlChasisReleasing(_rover, msPassed);
      do_roverCtrlHandReleasing(_rover, msPassed);
      do_roverCtrlArmReleasing(_rover, msPassed);
      if (msPassed > 6000)
      {
        fprintf(stderr, "*** DONE ***\n");
        _rover->m_state = StatePreparing;
        _rover->m_stateEntryTime.tv_sec = 0;
      }
      break;
      break;
  }

  return 0;
}
