#ifndef MEMORYPOOL__HPP__
#define MEMORYPOOL__HPP__

#include <cstdlib>
#include <cassert>
#include <mutex>

#ifdef _DEBUG
	#include <cstdio>
	#define printLog(...) printf("[%s : %d] ---> ", __FILE__, __LINE__); printf(__VA_ARGS__)
#else
	#define printLog
#endif // _DEBUG


#define MEMORY_POOL_BLOCK_MAX_SIZE 1024*11
class CMemoryPool;


struct MemBlock_ST
{
	int			nID;					//内存块编号
	int			nRef;					//内存块引用次数
	CMemoryPool	*pPool;;				//所属内存池
	MemBlock_ST	*nextBlock;				//next
	bool		bInPool;				//是否在内存池中
	char		reserved1;
	char		reserved2;
	char		reserved3;
};


class CMemoryPool
{
public:
	CMemoryPool()
	{
		m_pMemoryBuf = nullptr;
		m_pMemHeader = nullptr;
		m_BlockSize = 0;
		m_BlockCount = 0;
	}

	~CMemoryPool()
	{
		if (nullptr != m_pMemoryBuf)
		{
			free(m_pMemoryBuf);
			m_pMemoryBuf = nullptr;
			m_pMemHeader = nullptr;
		}
	}

	void Init()
	{
		printLog("Memory Pool(%llu * %llu) Init...\n", m_BlockSize, m_BlockCount);
		if (nullptr != m_pMemoryBuf)
			return;

		//内存池大小为 每个内存块大小 * 内存块个数
		m_pMemoryBuf = (char *)malloc(m_BlockCount * m_BlockSize);
		if (nullptr == m_pMemoryBuf)
		{
			printLog("Memory Pool Alloc Mem Failured!\n");
			return;
		}

		m_pMemHeader = (MemBlock_ST *)m_pMemoryBuf;
		m_pMemHeader->nID = 0;
		m_pMemHeader->nRef = 0;
		m_pMemHeader->bInPool = true;
		m_pMemHeader->nextBlock = nullptr;
		m_pMemHeader->pPool = this;

		MemBlock_ST *pTemp = m_pMemHeader;
		MemBlock_ST *pNext = nullptr;
		for (int i = 1; i < m_BlockCount; i++)
		{
			pNext = (MemBlock_ST *)(m_pMemoryBuf + (i * m_BlockSize));
			pNext->nID = i;
			pNext->nRef = 0;
			pNext->bInPool = true;
			pNext->nextBlock = nullptr;
			pNext->pPool = this;
			
			pTemp->nextBlock = pNext;
			pTemp = pNext;
		}
		return;
	}

	void* AllocMem(size_t nSize)
	{
		std::lock_guard<std::mutex> lg(m_mutexLock);
		if (nullptr == m_pMemoryBuf)
			Init();

		MemBlock_ST* pMem = nullptr;

		//内存池中无可用内存块，向系统申请
		if (nullptr == m_pMemHeader)
		{
			pMem = (MemBlock_ST*)malloc(sizeof(MemBlock_ST) + nSize);
			if (nullptr == pMem)
			{
				return pMem;
			}
			//内存块信息初始化
			pMem->nID = -1;
			pMem->nRef = 1;
			pMem->bInPool = false;
			pMem->nextBlock = nullptr;
			pMem->pPool = this;
		}
		else
		{
			pMem = m_pMemHeader;
			m_pMemHeader = m_pMemHeader->nextBlock;
			assert(pMem->nRef == 0);
			pMem->nRef = 1;
		}
		printLog("[ALLOC][POOL] ADDR:0x%llx, Id:%d， Size:%zu.\n", (size_t)pMem, pMem->nID, nSize);

		pMem = (MemBlock_ST*)((char*)pMem + sizeof(MemBlock_ST));
		return pMem;
	}

	void FreeMem(void *pBuf)
	{
		std::lock_guard<std::mutex> lg(m_mutexLock);
		if (nullptr == pBuf)
			return;

		MemBlock_ST* pBlock = (MemBlock_ST*)((char*)pBuf - sizeof(MemBlock_ST));
		if (pBlock->pPool != this)
			return;

		//这一块内存块是否被多次引用
		if (--pBlock->nRef != 0)
			return;
		 
		printLog("[FREE ][POOL] ADDR:0x%llx, Id:%d.\n", (size_t)pBlock, pBlock->nID);

		//内存块是否在内存池对象内
		if (pBlock->bInPool)
		{
			pBlock->nextBlock = m_pMemHeader;
			m_pMemHeader = pBlock;
		}
		else
		{
			free(pBlock);
		}
		return;
	}

protected:
	char		*m_pMemoryBuf;			//内存池首地址
	MemBlock_ST *m_pMemHeader;			//内存池首块地址
	size_t		m_BlockSize;			//内存池中每个块的大小，包含信息块信息信息加可用长度
	size_t		m_BlockCount;			//内存池中块的个数

	std::mutex m_mutexLock;
};


/*
使用模板类初始化内存池
其中内存池中每一块内存大小不包含内存控制块大小，内存控制块大小对外不可见
*/
template<size_t blockSize, size_t blockCount>
class CMemoryPoolAtor : public CMemoryPool
{
public:
	CMemoryPoolAtor()
	{
		m_BlockSize = blockSize + sizeof(MemBlock_ST);
		m_BlockCount = blockCount;

		//初始化内存池
		Init();
	}
};



/*
单例模式：
	1、饿汉模式（静态成员对象变量）
	2、懒汉模式（静态成员对象指针）
*/
class CMemoryManagement
{
private:
	CMemoryManagement()
	{
		Init(0, 64, &m_MemPool64);
		Init(65, 128, &m_MemPool128);
		Init(129, 256, &m_MemPool256);
		Init(257, 512, &m_MemPool512);
		Init(513, 1024, &m_MemPool1024);
		Init(1025, 1024*11, &m_MemPool1024_11);
	}

	~CMemoryManagement()
	{

	}

	void Init(int begin, int end, CMemoryPool* pPool)
	{
		if (begin < MEMORY_POOL_BLOCK_MAX_SIZE 
			&& end <= MEMORY_POOL_BLOCK_MAX_SIZE)
		{
			for (int i = begin; i <= end; i++)
			{
				m_pPoolArray[i] = pPool;
			}
		}
		return;
	}

public:
	static CMemoryManagement& Instance()
	{
		static CMemoryManagement _memoryMgn;
		return _memoryMgn;
	}

	void* Alloc(size_t nSize)
	{
		void* pMem = nullptr;

		//用户申请的内存大小大于最大的内存块大小，无法定位到内存池，需向系统申请
		if (nSize > MEMORY_POOL_BLOCK_MAX_SIZE)
		{
			pMem = malloc(nSize + sizeof(MemBlock_ST));

			MemBlock_ST* pBlock = (MemBlock_ST*)pMem;
			pBlock->nID = -1;
			pBlock->nRef = 1;
			pBlock->bInPool = false;
			pBlock->nextBlock = nullptr;
			pBlock->pPool = nullptr;

			printLog("[ALLOC][SYS] ADDR:0x%llx, Id:%d, Size:%zu.\n", (size_t)pBlock, pBlock->nID, nSize);

			pMem = (char*)pMem + sizeof(MemBlock_ST);
		}
		else
		{
			pMem = m_pPoolArray[nSize]->AllocMem(nSize);
		}
		
		return pMem;
	}

	void Free(void* pMem)
	{
		if (nullptr == pMem)
			return;

		MemBlock_ST* pBlock = (MemBlock_ST*)((char*)pMem - sizeof(MemBlock_ST));
		if (nullptr == pBlock->pPool)
		{
			printLog("[FREE ][SYS] ADDR:0x%llx, Id:%d.\n", (size_t)pBlock, pBlock->nID);
			if (--pBlock->nRef == 0)
				free(pBlock);
		} 
		else
		{
			pBlock->pPool->FreeMem(pMem);
		}

		return;
	}
	
private:
	CMemoryPool* m_pPoolArray[MEMORY_POOL_BLOCK_MAX_SIZE + 1];	//内存池映射数组，根据申请内存大小直接定位到可用内存池

	CMemoryPoolAtor<64, 1000> m_MemPool64;					//用户可用大小为64字节的内存池
	CMemoryPoolAtor<128, 500> m_MemPool128;					//用户可用大小为64 * 2字节的内存池
	CMemoryPoolAtor<256, 250> m_MemPool256;					//用户可用大小为64 * 4字节的内存池
	CMemoryPoolAtor<512, 125> m_MemPool512;					//用户可用大小为64 * 8字节的内存池
	CMemoryPoolAtor<1024, 60> m_MemPool1024;				//用户可用大小为64 * 16字节的内存池
	CMemoryPoolAtor<1024*11, 1000> m_MemPool1024_11;				//用户可用大小为64 * 16字节的内存池
	
};
#endif // MEMORYPOOL__HPP__
