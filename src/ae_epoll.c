#include <sys/epoll.h>

// 保存epoll的文件描述符
typedef struct aeApiState {
    int epfd; // 根据epoll_create(int size) 系统函数返回的文件描述符。用来epoll请求用的。
    struct epoll_event *events; // 用于向epoll要可执行数据时，赋值到这里。
} aeApiState;

// 创建epoll
static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state) return -1;
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize);
    if (!state->events) {
        zfree(state);
        return -1;
    }
    // epoll_create(int size)函数创建一个epoll的句柄（即返回一个描述符），用以后续添加、修改或删除需要监听的文件描述符。
    // 参数size用来告诉内核监听的数目一共有多大，这个参数不再起作用，只要大于0即可。在新的Linux内核中，epoll是动态分配的。
    state->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
    if (state->epfd == -1) {
        zfree(state->events);
        zfree(state);
        return -1;
    }
    anetCloexec(state->epfd);
    eventLoop->apidata = state;
    return 0;
}

static int aeApiResize(aeEventLoop *eventLoop, int setsize) {
    aeApiState *state = eventLoop->apidata;

    state->events = zrealloc(state->events, sizeof(struct epoll_event)*setsize);
    return 0;
}

static void aeApiFree(aeEventLoop *eventLoop) {
    aeApiState *state = eventLoop->apidata;

    close(state->epfd);
    zfree(state->events);
    zfree(state);
}

// 向epoll加事件
static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0}; /* avoid valgrind warning */
    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op = eventLoop->events[fd].mask == AE_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= eventLoop->events[fd].mask; /* Merge old events */
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    // epoll_ctl()是Linux中epoll机制中的一个函数，用于管理epoll的事件表。
    // 函数原型为：int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);。
    // 参数说明：
    // epfd: 是由epoll_create()函数返回的文件描述符。
    // op : 表示操作类型，可能的值是：EPOLL_CTL_ADD（注册新的fd）、EPOLL_CTL_MOD（修改已经注册的fd）、EPOLL_CTL_DEL（从epoll中移除一个fd）。
    // fd : 是需要操作的文件描述符。
    // event : 指向 epoll_event 的结构体变量，用于指定 fd 对应的事件。
    if (epoll_ctl(state->epfd,op,fd,&ee) == -1) return -1;
    return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask) {
    aeApiState *state = eventLoop->apidata;
    struct epoll_event ee = {0}; /* avoid valgrind warning */
    int mask = eventLoop->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN;
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (mask != AE_NONE) {
        epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
    }
}

// 从epoll获取可执行数据
// int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout); 
// 其中：
//      epfd ：是由epoll_create()产生的epoll专用的文件描述符。
//      events ：用于回传代处理事件的数组。
//      maxevents ：告之内核这个events有多大(数组成员的个数)。
//      timeout ：等待I/O事件发生的超时值（以毫秒为单位）。 0立即返回 -1一直等待 
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + (tvp->tv_usec + 999)/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;

            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE|AE_READABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE|AE_READABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}

static char *aeApiName(void) {
    return "epoll";
}
