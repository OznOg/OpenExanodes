/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/include/notifier.h"

#include <boost/bind.hpp>
#include <boost/bind/apply.hpp>
#include <errno.h>

#include "os/include/os_time.h"
#include "os/include/os_network.h"

#include <cstring>

using boost::apply;
using boost::shared_ptr;
using std::list;
using std::map;


class Notifier::Timer: private boost::noncopyable
{
public:
  Timer(VoidFunc _callback):
    callback(_callback)
  {
    assert(callback);
  }
  virtual ~Timer()
  {
  }

  const VoidFunc callback;
};




class SelectNotifier::Timer: public Notifier::Timer
{
public:
    Timer(unsigned int sec, VoidFunc _callback):
	Notifier::Timer(_callback)
	{
	    os_gettimeofday(&expiration);
	    expiration.tv_sec += sec;
	}

  void set_timeout(struct timeval *timeout) const
  {
    struct timeval now;

    assert(timeout);

    os_gettimeofday(&now);

    *timeout = os_timeval_diff(&expiration, &now);

    if (timeout->tv_sec < 0)
      memset(timeout, 0, sizeof(*timeout));

    assert(TIMEVAL_IS_VALID(timeout));
  }

  bool is_expired(const struct timeval &now) const
  {
    struct timeval left(os_timeval_diff(&expiration, &now));

    return left.tv_sec < 0 || (left.tv_sec == 0 && left.tv_usec == 0);
  }

private:
  friend class SelectNotifier::TimerComparator;

  struct timeval expiration;
};


/* This comparator is used to order the timers. One way to see this
 * function that helps understand what it is supposed to do is to
 * think of it as this question:
 *
 * Should the lhs timer expire later than the rhs timer?
 *
 * Also, to help keep the queue as short as possible, we would prefer
 * to have deleted timers put at the top of the heap, so that they get
 * cleaned up sooner.
 */
bool SelectNotifier::TimerComparator::operator()(
  const shared_ptr<Timer> &lhs, shared_ptr<Timer> &rhs) const
{
  /* If the left-hand side is gone, we say it is "bigger" to have it
   * cleaned up quicker. */
  if (!lhs)
    return false;

  /* If the right-hand side is gone, we say it is "bigger" to have it
   * cleaned up quicker. */
  if (!rhs)
    return true;

  /* If the lhs timer is bigger than the rhs timer, then it should
  * expire later. */
  if (lhs->expiration.tv_sec > rhs->expiration.tv_sec)
    return false;
  if (lhs->expiration.tv_sec < rhs->expiration.tv_sec)
    return true;

  return (lhs->expiration.tv_usec < rhs->expiration.tv_usec);
}


bool Notifier::done()
{
  return delayed_calls.empty();
}


void Notifier::delay_call(VoidFunc func)
{
  delayed_calls.push_back(func);
}


void Notifier::call_delayed()
{
  while (!delayed_calls.empty())
  {
    VoidFunc func(delayed_calls.front());

    delayed_calls.pop_front();

    func();
  }
}


static void prepare_select(int &nfds, fd_set *fds,
			   const map<int, Notifier::FdNotifyFunc> &watches)
{
  map<int, Notifier::FdNotifyFunc>::const_iterator it;

  FD_ZERO(fds);

  for (it = watches.begin(); it != watches.end(); ++it)
  {
    FD_SET(it->first, fds);
    nfds = std::max<int>(nfds, it->first + 1);
  }
}


static void analyze_select(int &rv, fd_set *fds,
			   const map<int, Notifier::FdNotifyFunc> &watches)
{
  map<int, Notifier::FdNotifyFunc>::const_iterator it;
  /* Keep the callbacks aside, since they could manipulate the
   * watch and invalidate our iterator. */
  list<Notifier::VoidFunc> callbacks;

  /* Using the the return value from select() allows us to
   * short-circuit looking needlessly at the rest of the watches (we
   * know they're not ready, we found the ready ones already). */
  for (it = watches.begin(); rv > 0 && it != watches.end(); ++it)
  {
    if (FD_ISSET(it->first, fds))
    {
      callbacks.push_back(bind(it->second, it->first));
      --rv;
    }
  }

  for_each(callbacks.begin(), callbacks.end(), apply<void>());
}


void SelectNotifier::run()
{
  while (!done())
  {
    int nfds(0);
    fd_set readfds, writefds;
    int rv;
    shared_ptr<Timer> nexttimer;
    struct timeval timeout;

    prepare_select(nfds, &readfds, watches_read);
    prepare_select(nfds, &writefds, watches_write);

    if (!timers.empty())
      nexttimer = timers.top();

    if (nexttimer)
      nexttimer->set_timeout(&timeout);

    rv = os_select(nfds, &readfds, &writefds, NULL, nexttimer ? &timeout : NULL);

    /* All of the errors select() returns are pretty lethal. The only
     * one that would be reasonable to handle is EBADF, but I would
     * give as a precondition that if one closes a file descriptor, he
     * should remove any and all watches on it first. */
    if (rv < 0 && rv != -EINTR)
	abort();

    analyze_select(rv, &readfds, watches_read);
    analyze_select(rv, &writefds, watches_write);

    struct timeval now;
    os_gettimeofday(&now);

    while (!timers.empty())
    {
      nexttimer = timers.top();

      if (!nexttimer)
      {
	timers.pop();
	continue;
      }

      if (nexttimer->is_expired(now))
      {
	nexttimer->callback();
	timers.pop();
      }
      else
	break;
    }
  }
}


void SelectNotifier::add_read(int fd, FdNotifyFunc func)
{
  watches_read.erase(fd);
  if (fd != -1)
    watches_read.insert(make_pair(fd, func));
}


void SelectNotifier::del_read(int fd)
{
  watches_read.erase(fd);
}


void SelectNotifier::add_write(int fd, FdNotifyFunc func)
{
  watches_write.erase(fd);
  if (fd != -1)
    watches_write.insert(make_pair(fd, func));
}


void SelectNotifier::del_write(int fd)
{
  watches_write.erase(fd);
}


shared_ptr<Notifier::Timer> SelectNotifier::get_timer(unsigned int msec,
						      VoidFunc func)
{
  shared_ptr<Timer> timer(new Timer(msec, func));

  timers.push(timer);

  return timer;
}

bool SelectNotifier::done()
{
  /* Do the delayed calls first, they might change the watches. */
  call_delayed();

  /* Remove any dead timers first.
   * when there is only one reference left on the timer, it means that
   * the only place when the timer still exists is in 'timers' strucutre.
   * This thus means that the timer can be safly deleted. */
  while (!timers.empty() && timers.top().use_count() == 1)
    timers.pop();

  return Notifier::done() && watches_read.empty() && watches_write.empty()
    && timers.empty();
}
