
#include "CTimestamp.hpp"
#include "MemoryAlloc.hpp"
#include <thread>
#include <iostream>
using namespace std;


const int nThreadCount = 4;
const int nLoopCount = 100;
const int mLoopCountForThread = nLoopCount / nThreadCount;

void workFunc(int index)
{
	char* data[mLoopCountForThread] = { 0 };

	for (int i = 0; i < mLoopCountForThread; i++)
	{
		data[i] = new char[1 +rand() % 1024];
	}
	for (int i = 0; i < mLoopCountForThread; i++)
	{
		delete data[i];
	}
}


int main()
{
	thread t[nThreadCount];

	CTimestamp time;
	for (int i = 0; i < nThreadCount; i++)
	{
		t[i] = thread(workFunc, i);
	}
	for (int i = 0; i < nThreadCount; i++)
	{
		t[i].join();
	}

	cout << "Time:" << time.getElapsedTimeInMilliSec() << endl;


	return 0;
}