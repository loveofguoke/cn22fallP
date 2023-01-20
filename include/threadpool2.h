#include <iostream>
#include <atomic>
#include <thread>
#include <future>
#include <mutex>
#include <queue>

class Task{
    public:
    Task():x(0), y(0){}
    Task(int a, int b): x(a), y(b){}
    int x;
    int y;
};

class ThreadSafeQueue{
    public:

    void push(const Task& task){
        std::lock_guard<std::mutex> lock_guard(mtx_);
        queue_.push(task);
    }
    bool pop(Task& task){
        std::lock_guard<std::mutex> lock_guard(mtx_);
        if(queue_.empty()) return false;
        task = queue_.front();
        queue_.pop();
        return true;
    }
    private:
    std::mutex mtx_;
    std::queue<Task> queue_;
};


class ThreadPool{
    public:
    ThreadPool(int num): threads_(num), stop_(false){}
    
    void run(){
        for(int i=0; i<threads_.size(); i++){
            threads_[i] = std::thread(&ThreadPool::running, this);
        }
    }

    void running(){
        while(!stop_){
            Task task;
            bool flag = queue_.pop(task);
            if(flag) {
                process_task(task);
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    void process_task(const Task& task){
        std::cout << task.x << " + " << task.y << " is " << (task.x+task.y) << std::endl;
    }

    void push(const Task& task){
        queue_.push(task);
    }

    void stop(){
        stop_ = true;
    }

    ~ThreadPool(){
        std::cout << "ThreadPool is quiting..." << std::endl;
        for(int i=0; i<threads_.size(); i++){
            threads_[i].join();
        }
        std::cout << "ThreadPool ends." << std::endl;
    }

    private:
    ThreadSafeQueue queue_;
    std::vector<std::thread> threads_;
    std::atomic<bool> stop_;
};

// Example:
// ThreadPool thread_pool(2);

// void test(){
//     thread_pool.push(Task{1,2});
//     thread_pool.push(Task{1,3});
//     thread_pool.push(Task{1,4});
//     thread_pool.push(Task{1,5});
//     thread_pool.push(Task{1,6});
//     thread_pool.push(Task{1,7});

// }

// int main(){
//     thread_pool.run();
//     test();
//     std::this_thread::sleep_for(std::chrono::seconds(3));
//     thread_pool.stop();
//     return 0;
// }