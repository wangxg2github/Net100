#ifndef HANDINGNETMSG__H__
#define HANDINGNETMSG__H__

#include <thread>
#include <mutex>
#include <list>

/*
 任务处理基类
 */
class CCellTask
{
public:
	CCellTask()
	{

	}

	virtual ~CCellTask()
	{

	}

	virtual void DoTask()
	{

	}

private:

};



class CCellTaskServer
{
private:
	std::mutex m_mutexAddTask;
	std::list<CCellTask *>  m_listTask;
	std::list<CCellTask *>  m_listTaskBuf;

public:
	CCellTaskServer()
	{

	}

	~CCellTaskServer()
	{

	}

	void AddTask(CCellTask *pTask)
	{
		std::lock_guard<std::mutex> lock(m_mutexAddTask);
		m_listTaskBuf.push_back(pTask);
		return;
	}

	void Start()
	{
		std::thread t(std::mem_fn(&CCellTaskServer::OnRuning), this);
		t.detach();
		return;
	}

	void OnRuning()
	{
		while (true)
		{
			//任务缓冲队列是否有数据，添加至任务队列
			if (!m_listTaskBuf.empty())
			{
				std::lock_guard<std::mutex> lock(m_mutexAddTask);
				for (auto pTask : m_listTaskBuf)
				{
					m_listTask.push_back(pTask);
				}
				m_listTaskBuf.clear();
			}

			//任务队列是否有数据
			if (m_listTask.empty())
			{
				std::chrono::milliseconds t(2);
				std::this_thread::sleep_for(t);
				continue;
			}

			//处理任务队列中的数据
			for (auto pTask : m_listTask)
			{
				pTask->DoTask();
			}
			m_listTask.clear();
		}
		return;
	}

};

#endif // !HANDINGNETMSG__H__

