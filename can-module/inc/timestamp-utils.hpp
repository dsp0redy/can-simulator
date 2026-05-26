#ifndef TIMESTAMP_UTILS_HPP
#define TIMESTAMP_UTILS_HPP

#include <ostream>
#include <sys/socket.h>

#ifndef SCM_TIMESTAMPING
#define SCM_TIMESTAMPING 37
#endif

inline const struct timespec *pickBestTimestamp(const struct timespec *ts)
{
    if (ts[2].tv_sec != 0 || ts[2].tv_nsec != 0)
    {
        return &ts[2];
    }
    if (ts[0].tv_sec != 0 || ts[0].tv_nsec != 0)
    {
        return &ts[0];
    }
    if (ts[1].tv_sec != 0 || ts[1].tv_nsec != 0)
    {
        return &ts[1];
    }
    return nullptr;
}

inline const struct timespec *findTimestampFromMsg(const struct msghdr &msg)
{
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(const_cast<struct msghdr *>(&msg), cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            const struct timespec *timestamps = reinterpret_cast<const struct timespec *>(CMSG_DATA(cmsg));
            return pickBestTimestamp(timestamps);
        }
    }
    return nullptr;
}

inline void printTimestampFromMsg(std::ostream &os, const struct msghdr &msg,
                                  const char *prefix = "", const char *suffix = "")
{
    const struct timespec *best = findTimestampFromMsg(msg);
    if (best != nullptr)
    {
        os << prefix << best->tv_sec << "s " << best->tv_nsec << "ns" << suffix;
    }
}

#endif // TIMESTAMP_UTILS_HPP