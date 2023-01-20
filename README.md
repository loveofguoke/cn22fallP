### How to run
使用Linux Socket编写，请在Linux环境中编译运行

以code为工作目录，在终端输入make，即可完成编译链接，生成可执行文件server

### Notice
在火狐浏览器中可以使用quit安全退出所有线程

但在谷歌浏览器中会出现**额外的socket连接**，占用线程且阻塞，所以暂时只能通过Ctrl+C退出

由于缺乏多线程编程的经验，所以线程池的实现参考了github中的一些项目