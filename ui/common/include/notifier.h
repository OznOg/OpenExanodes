/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __NOTIFIER_H__
#define __NOTIFIER_H__

#include <boost/noncopyable.hpp>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>


class Notifier: private boost::noncopyable
{
public:
  typedef std::function<void(int)> FdNotifyFunc;
  typedef std::function<void()> VoidFunc;

  class Timer;

  virtual void add_read(int fd, FdNotifyFunc func) = 0;
  virtual void del_read(int fd) = 0;

  virtual void add_write(int fd, FdNotifyFunc func) = 0;
  virtual void del_write(int fd) = 0;

  virtual std::shared_ptr<Timer> get_timer(unsigned int msec,
					     VoidFunc func) = 0;

  virtual bool done();

  void delay_call(VoidFunc func);
  void call_delayed();

  virtual ~Notifier() {}

private:
  std::list<VoidFunc> delayed_calls;
};


class SelectNotifier: public Notifier
{
public:
  void run();

  virtual void add_read(int fd, FdNotifyFunc func);
  virtual void del_read(int fd);
  virtual void add_write(int fd, FdNotifyFunc func);
  virtual void del_write(int fd);
  virtual std::shared_ptr<Timer> get_timer(unsigned int msec, VoidFunc func);
  virtual bool done();

private:
  class Timer;

  class TimerComparator
  {
  public:
    bool operator()(const std::shared_ptr<Timer> &lhs,
		    std::shared_ptr<Timer> &rhs) const;
  };

  std::map<int, FdNotifyFunc> watches_read;
  std::map<int, FdNotifyFunc> watches_write;
  std::priority_queue<std::shared_ptr<Timer>,
		      std::vector<std::shared_ptr<Timer> >,
		      TimerComparator> timers;
};


#endif /* __NOTIFIER_H__ */
